#pragma once

#include <fmt/core.h>

#include <array>
#include <memory>

#include "Balancer.hpp"

#include <exception/Error.hpp>
#include <prometheus/GetOrCreate.hpp>
#include <unsorted/Ewma.hpp>
#include <unsorted/Random.hpp>

namespace SD {

    class PeerState
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        unsigned     m_Success = 0;
        unsigned     m_Fail    = 0;
        Util::Ewma   m_SuccessRate;
        Util::EwmaTs m_Latency;

        enum class Zone
        {
            HEAL,
            RED,
            YELLOW,
            GREEN,
        };
        Zone   m_Zone        = Zone::YELLOW;
        time_t m_CurrentTime = 0;
        time_t m_Spent       = 0;

        Zone estimate()
        {
            m_Spent = 0;
            if (m_SuccessRate.estimate() > YELLOW_RATE)
                return Zone::GREEN;
            if (m_SuccessRate.estimate() > RED_RATE)
                return Zone::YELLOW;
            return Zone::RED;
        }

        void transition(time_t aNow)
        {
            if (m_Zone == Zone::HEAL) {
                if (m_Spent < HEAL_SECONDS)
                    return;
                m_Spent = 0;
                m_Zone = estimate();
                return;
            }
            if (m_Zone == Zone::RED) {
                if (m_Spent < COOLDOWN_SECONDS)
                    return;
                m_Zone  = Zone::HEAL;
                m_Spent = 0;
                m_SuccessRate.reset(HEAL_RATE);
                return;
            }
            m_Zone = estimate();
        }

        void prepare(time_t aNow)
        {
            if (aNow <= m_CurrentTime)
                return;
            m_CurrentTime = aNow;
            m_Spent++;

            if (m_Success + m_Fail > 0) {
                m_SuccessRate.add(m_Success / double(m_Success + m_Fail));
                m_Success = 0;
                m_Fail    = 0;
            }
            transition(aNow);
        }

    public:
        static constexpr double EWMA_FACTOR      = 0.95;
        static constexpr double HEAL_RATE        = 0.25;
        static constexpr double RED_RATE         = 0.50;
        static constexpr double INITIAL_RATE     = 1.0;
        static constexpr double YELLOW_RATE      = 0.75;
        static constexpr time_t COOLDOWN_SECONDS = 10;
        static constexpr time_t HEAL_SECONDS     = 10;
        static constexpr double INITIAL_LATENCY  = 0;

        PeerState()
        : m_SuccessRate(EWMA_FACTOR, INITIAL_RATE)
        , m_Latency(EWMA_FACTOR, INITIAL_LATENCY)
        {
        }

        void add(double aLatency, time_t aNow, bool aSuccess)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);
            if (aSuccess) {
                m_Success++;
                m_Latency.add(aLatency, aNow);
            } else
                m_Fail++;
        }

        bool test(time_t aNow)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);

            switch (m_Zone) {
            case Zone::RED:
                return false;
            case Zone::HEAL:
            case Zone::YELLOW:
                return Util::drand48() < m_SuccessRate.estimate();
            case Zone::GREEN:
            default:
                return true;
            }
        }

        PeerStat peer_stat() const
        {
            Lock sLock(m_Mutex);
            return {
                .success_rate = m_SuccessRate.estimate(),
                .latency      = m_Latency.estimate()};
        }

        void reset(double aRate, double aLatency)
        {
            Lock sLock(m_Mutex);
            m_Success = 0;
            m_Fail    = 0;
            m_SuccessRate.reset(aRate);
            m_Latency.reset(aLatency);
            m_Zone        = estimate();
            m_CurrentTime = 0;
            m_Spent       = 0;
        }
    };

    class Breaker : public PeerStatProvider
    {
        const std::string m_AVS;

        using Counter = Prometheus::Counter<>;
        Prometheus::GetOrCreate m_Metrics;

        mutable std::mutex m_Mutex;
        using Lock = std::unique_lock<std::mutex>;
        std::map<std::string, std::shared_ptr<PeerState>> m_State;

        std::shared_ptr<PeerState> getOrCreate(const std::string& aPeer)
        {
            Lock  sLock(m_Mutex);
            auto& sVal = m_State[aPeer];
            if (!sVal)
                sVal = std::make_shared<PeerState>();
            return sVal;
        }

        bool goodResponse(const asio_http::Response& aResponse)
        {
            auto sCode = aResponse.result_int();
            bool sBad  = sCode == 429 or sCode >= 500;
            return !sBad;
        }

        void tick(const std::string& aPeer, std::string_view aAction)
        {
            const auto sKey = fmt::format(R"(sd_request_count{{{},peer="{}",request="{}"}})",
                                          m_AVS, aPeer, aAction);
            m_Metrics.get<Counter>(sKey)->tick();
        }

    public:
        using Error = Exception::Error<Breaker>;

        Breaker(const std::string& aAVS)
        : m_AVS(aAVS)
        {
        }

        template <class T>
        asio_http::Response wrap(const std::string& aPeer, T&& aHandler, time_t aNow = time(nullptr))
        {
            static const std::string_view BLOCK("block");
            static const std::string_view PERMIT("permit");
            static const std::string_view FAIL("fail");

            auto sState = getOrCreate(aPeer);
            if (!sState->test(aNow)) {
                tick(aPeer, BLOCK);
                throw Error("request to " + aPeer + " blocked by circuit breaker");
            }

            try {
                tick(aPeer, PERMIT);
                Time::Meter         sMeter;
                asio_http::Response sResponse = aHandler();
                const double        sELA      = sMeter.get().to_double();
                if (goodResponse(sResponse)) {
                    sState->add(sELA, aNow, true);
                } else {
                    tick(aPeer, FAIL);
                    sState->add(0, aNow, false);
                }
                return sResponse;
            } catch (...) {
                tick(aPeer, FAIL);
                sState->add(0, aNow, false);
                throw;
            }
        }

        PeerStat peer_stat(const std::string& aPeer) override
        {
            return getOrCreate(aPeer)->peer_stat();
        }

        void reset(const std::string& aPeer, double aRate, double aLatency)
        {
            getOrCreate(aPeer)->reset(aRate, aLatency);
        }

        bool test(const std::string& aPeer, time_t aNow = time(nullptr))
        {
            return getOrCreate(aPeer)->test(aNow);
        }
    };

} // namespace SD