#pragma once

#include <mutex>

#include <boost/asio/steady_timer.hpp>

#include "Breaker.hpp"

#include <etcd/Etcd.hpp>
#include <parser/Json.hpp>
#include <unsorted/Random.hpp>

namespace SD {

    struct Balancer : public std::enable_shared_from_this<Balancer>
    {
        struct Params
        {
            Etcd::Client::Params addr;
            std::string          prefix;
            int                  period = 10;
            std::string          location;
            bool                 use_client_latency = true;
            bool                 use_server_rps     = false;
        };

        struct Entry
        {
            std::string key;
            double      weight   = 0;
            double      rps      = 0;
            uint32_t    threads  = 1;
            std::string location = {};

            void from_json(const ::Json::Value& aJson)
            {
                Parser::Json::from_object(aJson, "weight", weight);
                Parser::Json::from_object(aJson, "rps", rps);
                Parser::Json::from_object(aJson, "threads", threads);
                Parser::Json::from_object(aJson, "location", location);
            }
            double latency() const
            {
                return threads / weight;
            }
            double utilization() const
            {
                return rps / weight;
            }
        };

        using State = std::map<double, Entry>;
        using Error = std::runtime_error;

    private:
        const Params              m_Params;
        boost::asio::io_service&  m_Service;
        std::atomic<bool>         m_Stop{false};
        boost::asio::steady_timer m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        State              m_State;
        std::string        m_LastError;
        BreakerPtr         m_Breaker;

        struct History
        {
            double weight = 0;
        };
        std::map<std::string, History> m_History;

        void read_i(boost::asio::yield_context yield)
        {
            Etcd::Client       sClient(m_Service, m_Params.addr, yield);
            auto               sList = sClient.list(m_Params.prefix, 0);
            std::vector<Entry> sState;

            for (auto&& x : sList) {
                x.key.erase(0, m_Params.prefix.size());
                Json::Value sRoot;
                try {
                    sRoot = Parser::Json::parse(x.value);
                } catch (const std::invalid_argument& e) {
                    throw Error(std::string("SD: bad etcd server response: ") + e.what());
                }
                Entry sTmp;
                sTmp.key = x.key;
                Parser::Json::from_value(sRoot, sTmp);
                if (!m_Params.location.empty() and String::starts_with(sTmp.location, m_Params.location))
                    continue;
                sState.push_back(std::move(sTmp));
            }

            update(sState);
        }

        void adjust_weights(std::vector<Entry>& aState)
        {
            const double LOSS_MAX     = 0.5;
            const double LOSS_FACTOR  = 0.90;
            const double BOOST_FACTOR = 1.05;
            const double UTIL_MAX     = 0.75;

            for (auto& x : aState) {
                const auto   sStat           = m_Breaker->statistics(x.key);
                auto&        sHistory        = m_History[x.key];
                const double sPreviousWeight = sHistory.weight > 0 ? sHistory.weight : x.weight;
                double       sTargetWeight   = x.weight;
                if (m_Params.use_client_latency and sStat.latency > x.latency()) {
                    double sClientWeight = x.threads / sStat.latency;
                    sTargetWeight        = std::max(sClientWeight, sTargetWeight * LOSS_MAX);
                }
                if (m_Params.use_server_rps and x.utilization() > UTIL_MAX) {
                    double sUtil = std::min(x.utilization(), 1.);
                    sTargetWeight *= (1. + UTIL_MAX - sUtil);
                }
                if (sStat.success_rate < 0.95) {
                    sTargetWeight *= std::max(sStat.success_rate, LOSS_MAX);
                }
                if (sTargetWeight >= sPreviousWeight) {
                    sTargetWeight = std::min(sTargetWeight, sPreviousWeight * BOOST_FACTOR);
                } else {
                    sTargetWeight = std::max(sTargetWeight, sPreviousWeight * LOSS_FACTOR);
                }
                x.weight        = sTargetWeight;
                sHistory.weight = sTargetWeight;
            }
        }

#ifdef BOOST_TEST_MODULE
    public:
#endif
        // change aState
        void
        update(std::vector<Entry>& aState)
        {
            Lock lk(m_Mutex);
            if (m_Breaker)
                adjust_weights(aState);

            const double sTotalWeight = [&aState]() {
                double sSum = 0;
                for (auto& x : aState)
                    sSum += x.weight;
                return sSum;
            }();

            m_State.clear();
            double sWeight = 0;
            for (auto& x : aState) {
                sWeight += (x.weight / sTotalWeight);
                m_State[sWeight] = x;
            }
            m_LastError.clear();
        }

    private:
        void read(boost::asio::yield_context yield)
        {
            try {
                read_i(yield);
            } catch (const std::exception& e) {
                Lock lk(m_Mutex);
                m_LastError = e.what();
            }
        }

    public:
        Balancer(boost::asio::io_service& aService, const Params& aParams)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
        {
        }

        void with_breaker(BreakerPtr aBreaker)
        {
            Lock lk(m_Mutex);
            m_Breaker = aBreaker;
        }

        State state() const
        {
            Lock lk(m_Mutex);
            return m_State;
        }

        std::string random()
        {
            Lock lk(m_Mutex);
            if (m_State.empty())
                throw Error("SD: no peers available");
            std::string sPeer;
            if (auto sIt = m_State.lower_bound(Util::drand48()); sIt != m_State.end()) {
                sPeer = sIt->second.key;
            } else {
                sPeer = m_State.rbegin()->second.key;
            }
            if (m_Breaker)
                m_Breaker->ensure(sPeer);
            return sPeer;
        }

        std::string lastError() const
        {
            Lock lk(m_Mutex);
            return m_LastError;
        }

        std::future<bool> start()
        {
            auto sPromise = std::make_shared<std::promise<bool>>();
            boost::asio::spawn(
                m_Timer.get_executor(),
                [this, p = shared_from_this(), sPromise](boost::asio::yield_context yield) mutable {
                    boost::beast::error_code ec;
                    while (!m_Stop) {
                        read(yield);
                        if (sPromise) {
                            Lock lk(m_Mutex);
                            if (!m_State.empty()) {
                                sPromise->set_value(true);
                                sPromise.reset();
                            }
                        }
                        if (sPromise)
                            m_Timer.expires_from_now(std::chrono::milliseconds(100));
                        else
                            m_Timer.expires_from_now(std::chrono::seconds(m_Params.period));
                        m_Timer.async_wait(yield[ec]);
                    }
                    Lock lk(m_Mutex);
                    m_State.clear();
                    m_LastError = "stopped";
                });
            return sPromise->get_future();
        }

        void stop()
        {
            m_Stop = true;
            m_Timer.cancel();
        }
    };
} // namespace SD
