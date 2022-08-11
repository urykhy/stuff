#pragma once

#include <fmt/core.h>

#include <array>
#include <memory>

#include <exception/Error.hpp>
#include <prometheus/GetOrCreate.hpp>
#include <unsorted/Ewma.hpp>
#include <unsorted/Random.hpp>

namespace SD {

    class PeerState
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        unsigned   m_Success = 0;
        unsigned   m_Fail    = 0;
        Util::Ewma m_Ewma;

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
            if (m_Ewma.estimate() > YELLOW_RATE)
                return Zone::GREEN;
            if (m_Ewma.estimate() > RED_RATE)
                return Zone::YELLOW;
            return Zone::RED;
        }

        void transition(time_t aNow)
        {
            if (m_Zone == Zone::HEAL) {
                if (m_Spent < HEAL_SECONDS)
                    return;
                m_Zone  = estimate();
                return;
            }
            if (m_Zone == Zone::RED) {
                if (m_Spent < COOLDOWN_SECONDS)
                    return;
                m_Zone  = Zone::HEAL;
                m_Spent = 0;
                m_Ewma.reset(HEAL_RATE);
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
                m_Ewma.add(m_Success / double(m_Success + m_Fail));
                m_Success = 0;
                m_Fail    = 0;
            }
            transition(aNow);
        }

    public:
        static constexpr double EWMA_FACTOR      = 0.95;
        static constexpr double HEAL_RATE        = 0.25;
        static constexpr double RED_RATE         = 0.50;
        static constexpr double INITIAL_RATE     = 0.75;
        static constexpr double YELLOW_RATE      = 0.95;
        static constexpr time_t COOLDOWN_SECONDS = 10;
        static constexpr time_t HEAL_SECONDS     = 10;

        PeerState()
        : m_Ewma(EWMA_FACTOR, INITIAL_RATE)
        {
        }

        void insert(time_t aNow, bool aSuccess)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);
            if (aSuccess)
                m_Success++;
            else
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
                return Util::drand48() < m_Ewma.estimate();
            case Zone::GREEN:
            default:
                return true;
            }
        }

        double success_rate() const
        {
            Lock sLock(m_Mutex);
            return m_Ewma.estimate();
        }
    };

    class Breaker
    {
        const std::string m_API;
        std::atomic<bool> m_Stop{false};

        using Counter = Prometheus::Counter<>;
        Prometheus::GetOrCreate m_Metrics;

        mutable std::mutex m_Mutex;
        using Lock = std::unique_lock<std::mutex>;
        std::map<std::string, std::shared_ptr<PeerState>> m_State;

        std::shared_ptr<PeerState> getOrCreate(const std::string& aKey)
        {
            Lock  sLock(m_Mutex);
            auto& sVal = m_State[aKey];
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

        void tick(const std::string& aKey, std::string_view aAction)
        {
            const auto sKey = fmt::format(R"(sd_request_count{{api="{}",peer="{}",request="{}"}})",
                                          m_API, aKey, aAction);
            m_Metrics.get<Counter>(sKey)->tick();
        }

    public:
        using Error = Exception::Error<Breaker>;

        Breaker(const std::string& aAPI)
        : m_API(aAPI)
        {
        }

        template <class T>
        asio_http::Response wrap(const std::string& aKey, T&& aHandler)
        {
            static const std::string_view BLOCK("block");
            static const std::string_view PERMIT("permit");
            static const std::string_view FAIL("fail");

            auto sState = getOrCreate(aKey);
            if (!sState->test(time(nullptr))) {
                tick(aKey, BLOCK);
                throw Error("request to " + aKey + " blocked by circuit breaker");
            }
            try {
                tick(aKey, PERMIT);
                asio_http::Response sResponse = aHandler();
                if (goodResponse(sResponse)) {
                    sState->insert(time(nullptr), true);
                } else {
                    tick(aKey, FAIL);
                    sState->insert(time(nullptr), false);
                }
                return sResponse;
            } catch (...) {
                tick(aKey, FAIL);
                sState->insert(time(nullptr), false);
                throw;
            }
        }

        double success_rate(const std::string& aKey)
        {
            return getOrCreate(aKey)->success_rate();
        }
    };

    struct BreakerManager
    {
        using Ptr     = std::shared_ptr<Breaker>;
        using WeakPtr = std::weak_ptr<Breaker>;

    private:
        mutable std::mutex m_Mutex;
        using Lock = std::unique_lock<std::mutex>;
        std::map<std::string, WeakPtr> m_Info;

    public:
        Ptr get(const std::string& aAPI)
        {
            Lock sLock(m_Mutex);
            auto sIt = m_Info.find(aAPI);
            if (sIt != m_Info.end() and !sIt->second.expired())
                return sIt->second.lock();
            auto sNew    = std::make_shared<Breaker>(aAPI);
            m_Info[aAPI] = sNew;
            return sNew;
        }
    };

    inline BreakerManager::Ptr getBreaker(const std::string& aAPI)
    {
        static BreakerManager sManager;
        return sManager.get(aAPI);
    }

} // namespace SD