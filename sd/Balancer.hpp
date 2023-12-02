#pragma once

#include <cassert>
#include <mutex>
#include <numeric>

#include <boost/asio/steady_timer.hpp>

#include <etcd/Etcd.hpp>
#include <parser/Json.hpp>
#include <prometheus/Metrics.hpp>
#include <time/Meter.hpp>
#include <unsorted/Ewma.hpp>
#include <unsorted/Random.hpp>

namespace SD::Balancer {

    struct Params
    {
        Etcd::Client::Params addr                = {};
        std::string          prefix              = {};
        int                  period              = 10;
        std::string          location            = {};
        bool                 use_client_latency  = true;
        bool                 use_server_rps      = false;
        bool                 use_circuit_breaker = true;
        bool                 second_chance       = true;
        std::string          metrics_tags        = {};
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

    class ByWeight;
    using Error = Exception::Error<ByWeight>;

    struct Metrics
    {
        Prometheus::Counter<>       allowed;
        Prometheus::Counter<>       failed;
        Prometheus::Counter<>       blocked;
        Prometheus::Counter<>       closed;
        Prometheus::Counter<double> weight;

        Metrics(const Params& aParams, const std::string& aKey)
        : allowed("sd_call_count", aParams.metrics_tags, std::pair("peer", aKey), std::pair("kind","allowed"))
        , failed("sd_call_count", aParams.metrics_tags, std::pair("peer", aKey), std::pair("kind", "failed"))
        , blocked("sd_call_count", aParams.metrics_tags, std::pair("peer", aKey), std::pair("kind", "blocked"))
        , closed("sd_circuit_closed", aParams.metrics_tags, std::pair("peer", aKey))
        , weight("sd_weight", aParams.metrics_tags, std::pair("peer", aKey))
        {
        }
    };

    class Breaker
    {
        time_t m_Spent = 0;
        bool   m_Close = false;

    public:
        static constexpr double CLOSE_RATE = 0.50;
        static constexpr double PERIOD     = 10;

        // call ~ once a second
        void update(double aSuccessRate)
        {
            m_Spent++;
            if (m_Close and m_Spent > PERIOD) {
                reset();
                return;
            }
            if (!m_Close and m_Spent >= PERIOD and aSuccessRate < CLOSE_RATE) {
                m_Spent = 0;
                m_Close = 1;
            }
        }

        bool is_close() const
        {
            return m_Close;
        }

        void reset()
        {
            m_Spent = 0;
            m_Close = 0;
        }
    };

    class PeerInfo
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        friend class ByWeight;
        const Params      m_Params;
        const std::string m_Key;

        double m_Budget      = 0; // used in ByWeight
        double m_LastWeight  = 0;
        time_t m_CurrentTime = 0;

        Util::EwmaRps m_Ewma;
        Breaker       m_Breaker;
        Metrics       m_Metrics;

        void prepare(time_t aNow)
        {
            if (aNow <= m_CurrentTime)
                return;
            m_CurrentTime = aNow;

            if (m_Params.use_circuit_breaker) {
                const bool sOldClose = m_Breaker.is_close();
                m_Breaker.update(m_Ewma.estimate().success_rate);
                if (!sOldClose and m_Breaker.is_close())
                    m_Metrics.closed.tick();
            }
        }

        void adjust_weight(Entry& aEntry)
        {
            Lock         sLock(m_Mutex);
            const double LOSS_MAX     = 0.5;
            const double LOSS_FACTOR  = 0.90;
            const double BOOST_FACTOR = 1.05;
            const double UTIL_MAX     = 0.75;

            if (m_Breaker.is_close()) {
                aEntry.weight = 0;
                return;
            }

            const auto sEWMA = m_Ewma.estimate();

            const double sPreviousWeight = m_LastWeight > 0 ? m_LastWeight : aEntry.weight;
            double       sWeight         = aEntry.weight;
            // BOOST_TEST_MESSAGE("adjust " << aEntry.key << " success_rate " << sEWMA.success_rate << ", latency " << sEWMA.latency);
            if (m_Params.use_client_latency and sEWMA.latency > aEntry.latency()) {
                double sClientWeight = aEntry.threads / sEWMA.latency;
                sWeight              = std::max(sClientWeight, sWeight * LOSS_MAX);
            }
            if (m_Params.use_server_rps and aEntry.utilization() > UTIL_MAX) {
                double sUtil = std::min(aEntry.utilization(), 1.);
                sWeight *= (1. + UTIL_MAX - sUtil);
            }
            if (sEWMA.success_rate < 0.95) {
                sWeight *= std::max(sEWMA.success_rate, LOSS_MAX);
            }
            if (sWeight >= sPreviousWeight) {
                sWeight = std::min(sWeight, sPreviousWeight * BOOST_FACTOR);
            } else {
                sWeight = std::max(sWeight, sPreviousWeight * LOSS_FACTOR);
            }
            // BOOST_TEST_MESSAGE("adjust " << aEntry.key << " from " << aEntry.weight << " to " << sWeight);
            aEntry.weight = sWeight;
            m_LastWeight  = sWeight;
            m_Metrics.weight.set(sWeight);
        }

    public:
        PeerInfo(const Params& aParams, const std::string& aKey)
        : m_Params(aParams)
        , m_Key(aKey)
        , m_Metrics(aParams, aKey)
        {
        }

        static bool is_good_response(const asio_http::Response& aResponse)
        {
            auto sCode = aResponse.result_int();
            bool sBad  = sCode == 429 or sCode >= 500;
            return !sBad;
        }

        bool test(time_t aNow, bool aKeepAlive = false)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);
            if (m_Breaker.is_close() and !aKeepAlive)
                m_Metrics.blocked.tick();
            return !m_Breaker.is_close();
        }

        void add(double aLatency, time_t aNow, bool aSuccess)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);
            m_Metrics.allowed.tick();
            m_Ewma.add(aLatency, aNow, aSuccess);
            if (!aSuccess)
                m_Metrics.failed.tick();
        }

        asio_http::Response wrap(std::function<asio_http::Response()> aHandler, time_t aNow)
        {
            try {
                Time::Meter         sMeter;
                asio_http::Response sResponse = aHandler();
                const double        sELA      = sMeter.get().to_double();
                if (is_good_response(sResponse)) {
                    add(sELA, aNow, true);
                } else {
                    add(0, aNow, false);
                }
                return sResponse;
            } catch (...) {
                add(0, aNow, false);
                throw;
            }
        };

        const std::string& key() const { return m_Key; }

        void reset(double aLatency, double aSuccessRate)
        {
            Lock sLock(m_Mutex);
            m_CurrentTime = 0;
            m_Ewma.reset({aLatency, 0, aSuccessRate});
            m_Breaker.reset();
        }
    };
    using PeerInfoPtr = std::shared_ptr<PeerInfo>;

    class ByWeight
    {
#ifdef BOOST_TEST_MODULE
    public:
#endif
        const Params m_Params;

        double   m_RPS         = 0;
        double   m_TotalWeight = 0;
        double   m_Step        = 0;
        uint32_t m_Pos         = 0;
        time_t   m_LastTime    = 0;

        std::map<std::string, PeerInfoPtr> m_Store;
        std::vector<PeerInfoPtr>           m_Info;
        std::vector<Entry>                 m_Peers;

        void adjust_info(std::vector<Entry>& aState)
        {
            if (m_Info.size() != aState.size())
                m_Info.resize(aState.size());
            for (uint32_t i = 0; i < aState.size(); i++) {
                const std::string& sKey = aState[i].key;
                if (!m_Info[i] or m_Info[i]->m_Key != sKey) {
                    auto sIt = m_Store.find(sKey);
                    if (sIt == m_Store.end())
                        sIt = m_Store.insert(std::make_pair(sKey, std::make_shared<PeerInfo>(m_Params, sKey))).first;
                    m_Info[i] = sIt->second;
                }
            }
        }

        void adjust_weights(std::vector<Entry>& aState)
        {
            for (uint32_t i = 0; i < aState.size(); i++) {
                m_Info[i]->adjust_weight(aState[i]);
            }
        }

        void rewind()
        {
            for (uint32_t i = 0; i < m_Peers.size(); i++)
                m_Info[i]->m_Budget += m_Peers[i].weight;
        }

        uint32_t random_i()
        {
            while (true) {
                for (uint32_t i = 0; i < m_Peers.size(); i++) {
                    auto sPos = (m_Pos + i) % m_Peers.size();
                    if (m_Info[sPos]->m_Budget >= m_Step) {
                        assert(m_Info[sPos]->m_Key == m_Peers[sPos].key);
                        m_Info[sPos]->m_Budget -= m_Step;
                        m_Pos = sPos + 1;
                        return sPos;
                    }
                }
                rewind();
            }
        }

        bool allow_second_chance()
        {
            const double MAX_UTIL = 0.9;
            return m_Params.second_chance and size() > 1 and m_RPS / m_TotalWeight < MAX_UTIL;
        }

    public:
        ByWeight(const Params& aParams)
        : m_Params(aParams)
        {
        }

        void update(std::vector<Entry>&& aState)
        {
            m_Peers.clear();
            std::sort(aState.begin(),
                      aState.end(),
                      [](const auto& a, const auto& b) {
                          return a.key < b.key;
                      });

            adjust_info(aState);
            adjust_weights(aState);

            m_Peers = std::move(aState);
            aState.clear();

            for (uint32_t i = 0; i < m_Peers.size(); i++) {
                if (m_Info[i]->m_Budget > m_Peers[i].weight) {
                    m_Info[i]->m_Budget = m_Peers[i].weight;
                }
            }

            struct S
            {
                double rps          = 0;
                double total_weight = 0;
                double min_weight   = 0;
            } sStat;
            for (const auto& x : m_Peers) {
                sStat.rps += x.rps;
                sStat.total_weight += x.weight;
                if (sStat.min_weight > 0) {
                    sStat.min_weight = std::min(sStat.min_weight, x.weight);
                } else {
                    sStat.min_weight = x.weight;
                }
            }

            m_RPS         = sStat.rps;
            m_TotalWeight = sStat.total_weight;
            m_Step        = sStat.min_weight;
        }

        PeerInfoPtr random(time_t aNow)
        {
            if (aNow > m_LastTime) {
                // allow peer to recover from `CB closed` state
                for (uint32_t i = 0; i < m_Info.size(); i++)
                    m_Info[i]->test(aNow, true /* keep alive */);
                m_LastTime = aNow;
            }

            static const double MIN_WEIGHT = 0.0000001;
            if (empty())
                throw Error("SD: no peers available");
            if (m_TotalWeight < MIN_WEIGHT)
                throw Error("SD: no peers available");

            uint32_t sEntry = random_i();
            if (allow_second_chance()) {
                if (m_Info[sEntry]->test(aNow)) {
                    return m_Info[sEntry];
                } else {
                    sEntry = random_i();
                }
            }
            if (!m_Info[sEntry]->test(aNow))
                throw Error("SD: request to " + m_Peers[sEntry].key + " blocked by circuit breaker");

            return m_Info[sEntry];
        }

        std::vector<Entry> state() const { return m_Peers; }

        void   clear() { m_Peers.clear(); }
        bool   empty() const { return m_Peers.empty(); }
        size_t size() const { return m_Peers.size(); }
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
                    throw std::runtime_error(std::string("SD: bad etcd server response: ") + e.what());
                }
                Entry sTmp;
                sTmp.key = x.key;
                Parser::Json::from_value(sRoot, sTmp);
                if (!m_Params.location.empty() and !String::starts_with(sTmp.location, m_Params.location))
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

        std::vector<Entry> state() const
        {
            Lock lk(m_Mutex);
            return m_State.state();
        }

        PeerInfoPtr random(time_t aNow = time(nullptr))
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
