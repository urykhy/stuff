#pragma once

#include <chrono>
using namespace std::chrono_literals;

#include "Message.hpp"

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <threads/SafeQueue.hpp>
#include <unsorted/Env.hpp>
#include <unsorted/Log4cxx.hpp>

namespace Sentry {
    inline log4cxx::LoggerPtr sLogger = Logger::Get("sentry");

    struct Client : boost::noncopyable
    {
        struct Params
        {
            std::string url    = Util::getEnv("SENTRY_URL");
            std::string key    = Util::getEnv("SENTRY_KEY");
            std::string secret = Util::getEnv("SENTRY_SECRET");
            std::string client = "sentry++/1";
        };

    private:
        const Params          m_Sentry;
        Curl::Client::Default m_Hint;
        Curl::Client          m_Client;

    public:
        Client()
        {
            INFO("using " << m_Sentry.url);

            auto& sHeaders            = m_Hint.headers;
            sHeaders["Content-Type"]  = "application/json";
            sHeaders["X-Sentry-Auth"] = std::string("Sentry ") +
                                        "sentry_key=" + m_Sentry.key + ", " +
                                        "sentry_secret=" + m_Sentry.secret + ", " +
                                        "sentry_client=" + m_Sentry.client + ", " +
                                        "sentry_version=7";
        }
        Curl::Client::Result send(const Message& aMsg)
        {
            const std::string     sBody = aMsg.to_string();
            Curl::Client::Request sRequest{
                .method = Curl::Client::Method::POST,
                .url    = m_Sentry.url,
                .body   = sBody};
            return m_Client(m_Hint.wrap(std::move(sRequest)));
        }
    };

    class Queue : boost::noncopyable
    {
        Client                            m_Client;
        Threads::SafeQueueThread<Message> m_Queue;
        Threads::Group                    m_Group;
        static thread_local bool          m_Disabled;
        const unsigned                    m_Limit; // max notifications in queue

        void process(Message& aMsg)
        {
            m_Disabled = true;
            try {
                DEBUG("flush '" << aMsg.get_message() << "'");
                auto sResult = m_Client.send(aMsg);
                if (sResult.status != 200) {
                    ERROR(Exception::HttpError::format(sResult.body, sResult.status));
                }
            } catch (const std::exception& aErr) {
                ERROR(aErr.what());
            }
        }

    public:
        Queue(unsigned aLimit = 20)
        : m_Queue([this](Message& aMsg) { process(aMsg); }, {.retry = true, .linger = 2})
        , m_Limit(aLimit)
        {
        }
        void start()
        {
            m_Queue.start(m_Group);
        }
        void send(Message&& aMsg)
        {
            if (m_Queue.size() >= m_Limit)
                return;
            m_Queue.insert(std::move(aMsg));
            return;
        }
        bool is_disabled() const { return m_Disabled; }
    };

    inline thread_local bool Queue::m_Disabled{false};

} // namespace Sentry