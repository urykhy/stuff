#pragma once

#include <curl/Curl.hpp>
#include <threads/SafeQueue.hpp>
#include <time/Meter.hpp>
#include <unsorted/Log4cxx.hpp>

namespace MQ {
    struct Client
    {
        struct Params
        {
            std::string           url;
            std::string           client_id = "default";
            double                linger    = 5;
            Curl::Client::Default curl;
        };

    private:
        const Params m_Params;
        Curl::Client m_Client;

        struct Pair
        {
            std::string hash;
            std::string body;
        };
        using Queue = Threads::SafeQueueThread<Pair>;
        Queue m_Queue;

    public:
        Client(const Params& aParams)
        : m_Params(aParams)
        , m_Queue(
              [this](Pair& aPair) {
                  unsigned sCode = put(aPair.hash, aPair.body);
                  if (sCode != 200)
                      throw std::runtime_error("MQ::Client: remote error " + std::to_string(sCode));
              },
              Queue::Params{.retry = true})
        {
            if (m_Params.url.empty())
                throw std::invalid_argument("MQ::Client: empty url");
        }

        void start(Threads::Group& aGroup)
        {
            aGroup.at_stop([this]() {
                log4cxx::NDC   ndc("mq.client");
                Time::Deadline sDeadline(m_Params.linger);

                auto sPending = m_Queue.size();
                if (sPending > 0)
                    INFO("wait for " << sPending << " pending requests to complete");

                // wait until task queue processed
                while (!m_Queue.idle() and not sDeadline.expired())
                    Threads::sleep(0.01);

                sPending = m_Queue.size();
                if (sPending > 0)
                    WARN("lost " << sPending << " requests");
            });
            m_Queue.start(aGroup);
        }

        void insert(const std::string& aHash, const std::string& aBody)
        {
            m_Queue.insert({aHash, aBody});
        }

#ifdef BOOST_CHECK_EQUAL
    public:
#else
    private:
#endif
        unsigned put(const std::string& aHash, std::string_view aBody)
        {
            Curl::Client::Request sRequest{
                .method = Curl::Client::Method::PUT,
                .url    = m_Params.url + '?' + "client=" + m_Params.client_id + "&hash=" + aHash,
                .body   = aBody,
            };
            return m_Client(m_Params.curl.wrap(std::move(sRequest))).status;
        }
    };
} // namespace MQ