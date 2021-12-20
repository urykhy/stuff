#pragma once

#include <json/json.h>

#include <asio_http/Client.hpp>
#include <asio_http/Router.hpp>
#include <etcd/Etcd.hpp>
#include <threads/SafeQueue.hpp>
#include <time/Meter.hpp>
#include <unsorted/Log4cxx.hpp>

namespace MQ {
    struct Server
    {
        struct Params
        {
            Etcd::Client::Params etcd;
            std::string          endpoint    = "/mq";
            unsigned             queue_limit = 10;          // max number of pending tasks in queue before http error 429 (too many requests)
            unsigned             etcd_limit  = 10;          // max number of hashes per client in etcd
            double               linger      = 5;           // max time to wait until all tasks processed
            unsigned             max_size    = 1024 * 1024; // max upload size. return http error 413 (payload too large)
        };

    private:
        const Params                          m_Params;
        Threads::SafeQueueThread<std::string> m_Queue;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        bool               m_Active{true};
        uint32_t           m_Running{0};

        using Error = std::runtime_error;

        Json::Value parseState(const std::string& aState)
        {
            Json::Value sResponse;
            try {
                sResponse = Parser::Json::parse(aState);
            } catch (const std::invalid_argument& e) {
                throw Error(std::string("mq.server: bad server response: ") + e.what());
            }
            if (!sResponse.isArray())
                throw Error("mq.server: bad server response: array expected");
            return sResponse;
        }

        std::string formatState(const Json::Value& aJson)
        {
            Json::StreamWriterBuilder sBuilder;
            return Json::writeString(sBuilder, aJson);
        }

        Json::Value etcd_request(asio_http::asio::io_service& aService, const std::string& aAPI, std::string&& aBody, asio_http::asio::yield_context yield)
        {
            auto sResult = asio_http::async(
                               aService,
                               {.method  = asio_http::http::verb::post,
                                .url     = m_Params.etcd.url + "/v3/" + aAPI,
                                .body    = std::move(aBody),
                                .headers = {
                                    {asio_http::Headers::Accept, "application/json"},
                                    {asio_http::Headers::ContentType, "application/json"}}},
                               yield)
                               .get();
            return Etcd::Protocol::parseResponse(static_cast<unsigned>(sResult.result()), sResult.body());
        }

        void process_i(asio_http::asio::io_service& aService, const std::string& aClient, const std::string& aHash, const std::string& aBody, asio_http::asio::yield_context yield)
        {
            const std::string sKey     = m_Params.etcd.prefix + aClient;
            bool              sInitial = true;
            Json::Value       sState;
            Json::Value       sResponse = etcd_request(aService, "kv/range", Etcd::Protocol::get(sKey), yield);
            const std::string sOld      = Parser::Base64(sResponse["kvs"][0]["value"].asString());

            if (!sOld.empty()) {
                sInitial = false;
                sState   = parseState(sOld);
            }

            for (Json::Value::ArrayIndex i = 0; i != sState.size(); i++)
                if (sState[i].asString() == aHash) {
                    INFO("duplicate block " << aHash);
                    return;
                }

            sState[sState.size()] = aHash;

            while (sState.size() > m_Params.etcd_limit) {
                Json::Value sTmp;
                sState.removeIndex(0, &sTmp);
            }

            if (sInitial) {
                auto sResponse = etcd_request(aService, "kv/txn", Etcd::Protocol::atomicPut(sKey, formatState(sState)), yield);
                Etcd::Protocol::checkTxnResponse(sResponse);
            } else {
                auto sResponse = etcd_request(aService, "kv/txn", Etcd::Protocol::atomicUpdate(sKey, sOld, formatState(sState)), yield);
                Etcd::Protocol::checkTxnResponse(sResponse);
            }
            DEBUG("accept block " << aHash);

            m_Queue.insert(aBody);
        }

        void process(asio_http::asio::io_service& aService, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield)
        {
            namespace http = boost::beast::http;

            // need mutex, to check m_Active and tick m_Running at once
            {
                Lock sLock(m_Mutex);
                if (!m_Active) {
                    aResponse.result(http::status::service_unavailable);
                    return;
                }
                m_Running++; // number of current requests
            }

            Util::Raii sCleanup([this]() {
                Lock sLock(m_Mutex);
                m_Running--;
            });

            if (m_Queue.size() > m_Params.queue_limit) {
                aResponse.result(http::status::too_many_requests);
                return;
            }

            auto sQuery = aRequest.target();
            if (sQuery.size() > m_Params.max_size) {
                aResponse.result(http::status::payload_too_large);
                return;
            }

            std::string sClient;
            std::string sHash;
            Parser::http_query(std::string_view(sQuery.data(), sQuery.size()), [&sClient, &sHash](auto aName, auto aValue) {
                if (aName == "client")
                    sClient.assign(aValue.data(), aValue.size());
                if (aName == "hash")
                    sHash.assign(aValue.data(), aValue.size());
            });
            if (sClient.empty() or sHash.empty()) {
                aResponse.result(http::status::bad_request);
                return;
            }

            log4cxx::NDC ndc("client:" + sClient);
            try {
                process_i(aService, sClient, sHash, aRequest.body(), yield);
                aResponse.result(http::status::ok);
            } catch (const std::exception& e) {
                WARN("fail to process: " << e.what());
                aResponse.result(http::status::bad_gateway);
            }
        }

        bool isRunning() const
        {
            Lock sLock(m_Mutex);
            return m_Running > 0;
        }

    public:
        Server(const Params& aParams, std::function<void(std::string&)> aHandler)
        : m_Params(aParams)
        , m_Queue(aHandler)
        {}

        void configure(asio_http::RouterPtr aRouter)
        {
            namespace http = boost::beast::http;
            aRouter->insert(m_Params.endpoint, [this](asio_http::asio::io_service& aService, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
                if (aRequest.method() != http::verb::put) {
                    aResponse.result(http::status::method_not_allowed);
                    return;
                }
                log4cxx::NDC ndc("mq.server");
                process(aService, aRequest, aResponse, yield);
            });
        }

        void start(Threads::Group& aGroup)
        {
            aGroup.at_stop([this]() {
                log4cxx::NDC ndc("mq.server");
                {
                    Lock sLock(m_Mutex);
                    m_Active = false; // stop accepting new requests
                }
                Time::Deadline sDeadline(m_Params.linger);

                auto sPending = m_Queue.size();
                if (sPending > 0)
                    INFO("wait for " << sPending << " pending requests to complete");

                // wait to complete current requests
                while (isRunning() and not sDeadline.expired())
                    Threads::sleep(0.01);

                // wait until all tasks processed
                while (!m_Queue.idle() and not sDeadline.expired())
                    Threads::sleep(0.01);

                sPending = m_Queue.size();
                if (sPending > 0)
                    WARN("lost" << sPending << " requests");
            });
            m_Queue.start(aGroup);
        }
    };
} // namespace MQ