#pragma once

#include <mutex>
#include <numeric>

#include <boost/asio/steady_timer.hpp>

#include "Breaker.hpp"

#include <etcd/Etcd.hpp>
#include <parser/Json.hpp>
#include <unsorted/Random.hpp>

namespace SD::Balancer {

    using Error = std::runtime_error;

    struct Params
    {
        Etcd::Client::Params addr               = {};
        std::string          prefix             = {};
        int                  period             = 10;
        std::string          location           = {};
        bool                 use_client_latency = true;
        bool                 use_server_rps     = false;
        bool                 second_chance      = false;
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
            // TODO: success_rate ?
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

    class ByWeight
    {
#ifdef BOOST_TEST_MODULE
    public:
#endif
        const Params m_Params;
        BreakerPtr   m_Breaker;

        double m_RPS         = 0;
        double m_TotalWeight = 0;

        std::map<double, Entry> m_State;

        struct History
        {
            double weight = 0;
        };
        std::map<std::string, History> m_History;

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

        using EntryW = std::pair<const Entry*, double>;
        EntryW random_i(double aRandom = Util::drand48()) const
        {
            if (auto sIt = m_State.lower_bound(aRandom); sIt != m_State.end()) {
                return EntryW{&sIt->second, aRandom};
            } else {
                return EntryW{&m_State.rbegin()->second, aRandom};
            }
        }

        bool allow_second_chance()
        {
            const double MAX_UTIL = 0.9;
            return m_Params.second_chance and size() > 1 and m_RPS / m_TotalWeight < MAX_UTIL;
        }

        EntryW second_chance(double aRelative, double aPos) const
        {
            double sRandom = Util::drand48() * (1 - aRelative);
            if (sRandom >= aPos)
                sRandom += aRelative;
            return random_i(sRandom);
        }

    public:
        ByWeight(const Params& aParams)
        : m_Params(aParams)
        {
        }

        void with_breaker(BreakerPtr aBreaker)
        {
            m_Breaker = aBreaker;
        }

        void update(std::vector<Entry>&& aState)
        {
            if (m_Breaker)
                adjust_weights(aState);

            struct S
            {
                double rps    = 0;
                double weight = 0;
            };
            auto [sRPS, sTotalWeight] = std::accumulate(
                aState.begin(),
                aState.end(),
                S{},
                [](const S& a, Entry& e) {
                    S sStat = a;
                    sStat.rps += e.rps;
                    sStat.weight += e.weight;
                    return sStat;
                });
            // BOOST_TEST_MESSAGE("XXX: rps: " << sRPS << ", avail rps: " << sAvailRPS << ", total weight: " << sTotalWeight);

            m_State.clear();
            double sWeight = 0;
            for (auto& x : aState) {
                sWeight += (x.weight / sTotalWeight);
                m_State[sWeight] = x;
            }
            m_RPS         = sRPS;
            m_TotalWeight = sTotalWeight;
        }

        std::string random(time_t aNow)
        {
            if (m_State.empty())
                throw Error("SD: no peers available");
            auto [sEntry, sWeight] = random_i();
            if (m_Breaker) {
                if (allow_second_chance()) {
                    if (m_Breaker->test(sEntry->key, aNow)) {
                        return sEntry->key;
                    } else {
                        sEntry = second_chance(sEntry->weight / m_TotalWeight, sWeight).first;
                    }
                }
                if (!m_Breaker->test(sEntry->key, aNow))
                    throw Breaker::Error("SD: request to " + sEntry->key + " blocked by circuit breaker");
            }
            return sEntry->key;
        }

        std::map<double, Entry> state() const { return m_State; }

        void   clear() { m_State.clear(); }
        bool   empty() const { return m_State.empty(); }
        size_t size() const { return m_State.size(); }
    };

    class Engine : public std::enable_shared_from_this<Engine>
    {
        const Params              m_Params;
        boost::asio::io_service&  m_Service;
        std::atomic<bool>         m_Stop{false};
        boost::asio::steady_timer m_Timer;

#ifdef BOOST_TEST_MODULE
    public:
#endif
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        ByWeight           m_State;

        std::string m_LastError;

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

            update(std::move(sState));
        }

        void update(std::vector<Entry>&& aState)
        {
            Lock lk(m_Mutex);
            m_State.update(std::move(aState));
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
        Engine(boost::asio::io_service& aService, const Params& aParams)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
        , m_State(aParams)
        {
        }

        void with_breaker(BreakerPtr aBreaker)
        {
            Lock lk(m_Mutex);
            m_State.with_breaker(aBreaker);
        }

        std::map<double, Entry> state() const
        {
            Lock lk(m_Mutex);
            return m_State.state();
        }

        std::string random(time_t aNow = time(nullptr))
        {
            Lock lk(m_Mutex);
            return m_State.random(aNow);
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
} // namespace SD::Balancer
