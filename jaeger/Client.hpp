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
        Curl::Client::Result send(const Trace& aMsg) { return send(aMsg.to_string()); }
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
        Client                                m_Client;
        Threads::SafeQueueThread<std::string> m_Queue;
        Threads::Group                        m_Group;
        const unsigned                        m_Limit; // max notifications in queue

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

    public:
        Queue(unsigned aLimit = 20)
        : m_Queue([this](const std::string& aMsg) { process(aMsg); }, {.retry = true, .linger = 2})
        , m_Limit(aLimit)
        {
        }
        void start()
        {
            m_Queue.start(m_Group);
        }
        bool send(const Trace& aMsg)
        {
            if (m_Queue.size() >= m_Limit)
                return false;
            m_Queue.insert(aMsg.to_string());
            return true;
        }
    };
} // namespace Jaeger