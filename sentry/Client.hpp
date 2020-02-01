#pragma once

#include <curl/Curl.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include "Message.hpp"
#include <threads/SafeQueue.hpp>

namespace Sentry
{
    struct Client
    {
        struct Params {
            std::string url;
            std::string key;
            std::string secret;
            std::string client = "sentry++/1";
        };
    private:

        const Params m_Sentry;
        Curl::Client::Params m_Params;
        Curl::Client m_Client;
    public:

        Client(const Params& aSentry)
        : m_Sentry(aSentry)
        , m_Client(m_Params)
        {
            using Header = Curl::Client::Params::Header;
            m_Params.headers.emplace_back(Header{"Content-Type","application/json"});
            m_Params.headers.emplace_back(Header{"X-Sentry-Auth",std::string("Sentry ")
                + "sentry_key="    + m_Sentry.key + ", "
                + "sentry_secret=" + m_Sentry.secret + ", "
                + "sentry_client=" + m_Sentry.client + ", "
                + "sentry_version=7"
            });
        }
        Curl::Client::Result send(const Message& aMsg) { return send(aMsg.to_string()); }
        Curl::Client::Result send(const std::string& aMsg) { return m_Client.POST(m_Sentry.url, aMsg); }
    };

    class Queue
    {
        Client m_Client;
        Threads::SafeQueueThread<std::string> m_Queue;
        Threads::Group m_Group;
    public:
        Queue(const Client::Params& aSentry)
        : m_Client(aSentry)
        , m_Queue([this](const std::string& aMsg) { m_Client.send(aMsg); })
        {}
        ~Queue() throw()
        {
            enum { MAX_WAIT = 20 }; // wait a bit if not all events sent
            for (int i = 0; i < MAX_WAIT and !m_Queue.idle(); i++)
                std::this_thread::sleep_for(100ms);
        }
        void start() { m_Queue.start(m_Group); }
        void send(const Message& aMsg) { m_Queue.insert(aMsg.to_string()); }
    };

} // namespace Sentry