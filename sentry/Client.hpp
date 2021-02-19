#pragma once

#include <chrono>

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <unsorted/Log4cxx.hpp>
using namespace std::chrono_literals;

#include <threads/SafeQueue.hpp>

#include "Message.hpp"

namespace Sentry {
    struct Client
    {
        struct Params
        {
            std::string url;
            std::string key;
            std::string secret;
            std::string client = "sentry++/1";
        };

    private:
        const Params                  m_Sentry;
        std::unique_ptr<Curl::Client> m_Client;

    public:
        Client(const Params& aSentry)
        : m_Sentry(aSentry)
        {
            Curl::Client::Params sParams;

            auto& sHeaders            = sParams.headers;
            sHeaders["Content-Type"]  = "application/json";
            sHeaders["X-Sentry-Auth"] = std::string("Sentry ") +
                                        "sentry_key=" + m_Sentry.key + ", " +
                                        "sentry_secret=" + m_Sentry.secret + ", " +
                                        "sentry_client=" + m_Sentry.client + ", " +
                                        "sentry_version=7";
            m_Client = std::make_unique<Curl::Client>(sParams);
        }
        Curl::Client::Result send(const Message& aMsg) { return send(aMsg.to_string()); }
        Curl::Client::Result send(std::string_view aMsg)
        {
            const Curl::Client::Request sRequest{
                .method = Curl::Client::Method::POST,
                .url    = m_Sentry.url,
                .body   = aMsg};
            return m_Client->operator()(sRequest);
        }
    };

    class Queue
    {
        Client                                m_Client;
        Threads::SafeQueueThread<std::string> m_Queue;
        Threads::Group                        m_Group;
        static thread_local bool              m_Disabled;

        void process(const std::string& aMsg)
        {
            m_Disabled = true;
            log4cxx::NDC ndc("sentry");
            try {
                auto sResult = m_Client.send(aMsg);
                if (sResult.status != 200)
                    ERROR("notification: " << Exception::HttpError::format(sResult.body, sResult.status));
            } catch (const std::exception& aErr) {
                ERROR("notification: " << aErr.what());
            }
        }

    public:
        Queue(const Client::Params& aSentry)
        : m_Client(aSentry)
        , m_Queue([this](const std::string& aMsg) { process(aMsg); })
        {}
        void start()
        {
            m_Queue.start(m_Group);
            m_Group.at_stop([this]() {
                constexpr time_t MAX_WAIT = 20; // wait a bit if not all events sent
                for (time_t i = 0; i < MAX_WAIT and !m_Queue.idle(); i++)
                    std::this_thread::sleep_for(100ms);
            });
        }
        void send(const Message& aMsg) { m_Queue.insert(aMsg.to_string()); }
        bool is_disabled() const { return m_Disabled; }
    };

    inline thread_local bool Queue::m_Disabled{false};

} // namespace Sentry