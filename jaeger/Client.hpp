#pragma once

#include "Jaeger.hpp"

#include <curl/Curl.hpp>
#include <threads/SafeQueue.hpp>
#include <unsorted/Env.hpp>
#include <unsorted/Log4cxx.hpp>

namespace Jaeger {
    struct Client : boost::noncopyable
    {
        struct Params
        {
            std::string url = Util::getEnv("JAEGER_URL");
        };

    private:
        const Params          m_Params;
        Curl::Client::Default m_Hint;
        Curl::Client          m_Client;

    public:
        Client()
        {
            auto& sHeaders           = m_Hint.headers;
            sHeaders["Content-Type"] = "application/x-protobuf";
        }
        Curl::Client::Result send(std::string_view aMsg)
        {
            Curl::Client::Request sRequest{
                .method = Curl::Client::Method::POST,
                .url    = m_Params.url,
                .body   = aMsg};
            return m_Client(m_Hint.wrap(std::move(sRequest)));
        }
    };

    class Queue : boost::noncopyable
    {
        const std::string         m_Service;
        const std::string         m_Version;
        static constexpr unsigned QUEUE_LIMIT = 20;   // max notifications in queue
        static constexpr int      SPAN_LIMIT  = 1000; // max traces in single batch

        std::mutex                                      m_Mutex;
        opentelemetry::proto::trace::v1::TracesData     m_Traces;
        opentelemetry::proto::trace::v1::ResourceSpans* m_Spans = nullptr;

        Client                                m_Client;
        Threads::SafeQueueThread<std::string> m_Queue;
        Threads::Group                        m_Group; // must be last, to be destroyed first

        void push_i()
        {
            m_Queue.insert(m_Traces.SerializeAsString());
            m_Spans = nullptr;
            m_Traces.Clear();
        }

        void idle()
        {
            std::unique_lock sLock(m_Mutex);
            if (m_Queue.size() == 0 and m_Spans != nullptr)
                push_i();
        }

        void process(const std::string& aMsg)
        {
            log4cxx::NDC ndc("jaeger");
            try {
                auto sResult = m_Client.send(aMsg);
                if (sResult.status != 200)
                    ERROR("notification: " << Exception::HttpError::format(sResult.body, sResult.status));
            } catch (const std::exception& aErr) {
                ERROR("notification: " << aErr.what());
            }
        }

        void set_tag(opentelemetry::proto::common::v1::KeyValue* aTag, const std::string& aKey, const std::string& aValue)
        {
            aTag->set_key(aKey);
            aTag->mutable_value()->set_string_value(aValue);
        }

    public:
        Queue(const std::string& aService, const std::string aVersion)
        : m_Service(aService)
        , m_Version(aVersion)
        , m_Queue([this](const std::string& aMsg) { process(aMsg); },
                  {.retry  = true,
                   .linger = 2,
                   .idle   = [this]() { idle(); }})
        {
        }
        void start()
        {
            m_Group.at_stop([this]() {
                std::unique_lock sLock(m_Mutex);
                if (m_Spans != nullptr)
                    push_i();
            });
            m_Queue.start(m_Group);
        }
        bool send(Trace& aMsg)
        {
            std::unique_lock sLock(m_Mutex);

            if (m_Queue.size() >= QUEUE_LIMIT)
                return false;

            if (m_Spans == nullptr) {
                m_Spans = m_Traces.add_resource_spans();
                set_tag(m_Spans->mutable_resource()->add_attributes(), "service.name", m_Service);
                set_tag(m_Spans->mutable_resource()->add_attributes(), "service.version", m_Version);
            }

            auto sScope = m_Spans->add_scope_spans();
            *sScope     = std::move(aMsg.spans());

            if (m_Spans->scope_spans_size() >= SPAN_LIMIT)
                push_i();

            return true;
        }
    };
} // namespace Jaeger
