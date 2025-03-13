#pragma once

#include "Jaeger.hpp"

#include <curl/Curl.hpp>
#include <threads/SafeQueue.hpp>
#include <unsorted/Env.hpp>
#include <unsorted/Log4cxx.hpp>

namespace Jaeger {
    inline log4cxx::LoggerPtr sLogger = Logger::Get("jaeger");

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
            INFO("using " << m_Params.url);
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

    class Queue : public QueueFace
    {
        const std::string         m_Service;
        const std::string         m_Version;
        static constexpr unsigned QUEUE_LIMIT = 20;   // max notifications in queue
        static constexpr int      SPAN_LIMIT  = 1000; // max traces in single batch

        std::mutex                                      m_Mutex;
        trace::TracesData     m_Traces;
        trace::ResourceSpans* m_Spans     = nullptr;
        int                                             m_SpanCount = 0;

        struct Info
        {
            std::string serialized = {};
            int         count      = {};
        };

        Client                         m_Client;
        Threads::SafeQueueThread<Info> m_Queue;
        Threads::Group                 m_Group; // must be last, to be destroyed first

        void push_i()
        {
            assert(m_Spans);
            m_Queue.insert(Info{m_Traces.SerializeAsString(), m_SpanCount});
            m_Spans     = nullptr;
            m_SpanCount = 0;
            m_Traces.Clear();
        }

        void idle()
        {
            std::unique_lock sLock(m_Mutex);
            if (m_Queue.size() == 0 and m_Spans != nullptr)
                push_i();
        }

        void process(const Info& aInfo)
        {
            try {
                DEBUG("flush " << aInfo.count << " spans");
                auto sResult = m_Client.send(aInfo.serialized);
                if (sResult.status != 200)
                    ERROR(Exception::HttpError::format(sResult.body, sResult.status));
            } catch (const std::exception& aErr) {
                ERROR(aErr.what());
            }
        }

        void set_tag(common::KeyValue* aTag, const std::string& aKey, const std::string& aValue)
        {
            aTag->set_key(aKey);
            aTag->mutable_value()->set_string_value(aValue);
        }

    public:
        Queue(const std::string& aService, const std::string aVersion)
        : m_Service(aService)
        , m_Version(aVersion)
        , m_Queue([this](const Info& aInfo) { process(aInfo); },
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

        void send(trace::ScopeSpans& aMsg) noexcept override
        {
            const int sCount = aMsg.spans_size();
            try {
                std::unique_lock sLock(m_Mutex);

                if (m_Queue.size() >= QUEUE_LIMIT) {
                    ERROR("fail to enqueue " << sCount << " spans: queue limit");
                    return;
                }

                if (m_Spans == nullptr) {
                    m_Spans = m_Traces.add_resource_spans();
                    set_tag(m_Spans->mutable_resource()->add_attributes(), "service.name", m_Service);
                    set_tag(m_Spans->mutable_resource()->add_attributes(), "service.version", m_Version);
                }

                auto sScope = m_Spans->add_scope_spans();
                *sScope     = std::move(aMsg);
                m_SpanCount += sCount;

                if (m_SpanCount >= SPAN_LIMIT)
                    push_i();
            } catch (const std::exception& e) {
                ERROR("fail to enqueue " << sCount << " spans: " << e.what());
            }
        }
    };
} // namespace Jaeger
