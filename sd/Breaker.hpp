#pragma once

#include <array>
#include <memory>

#include <boost/asio/steady_timer.hpp>

#include <exception/Error.hpp>
#include <unsorted/Random.hpp>

namespace SD {

    struct PeerState
    {
        struct Duration
        {
            time_t green  = 0;
            time_t yellow = 0;
            time_t red    = 0;
            time_t heal   = 0;
        };

    private:
        class Bucket
        {
            uint32_t m_Success = 0;
            uint32_t m_Calls   = 0;

        public:
            void insert(bool aSuccess)
            {
                if (aSuccess)
                    m_Success++;
                m_Calls++;
            }
            double get() const
            {
                return m_Calls > 0 ? m_Success / (double)m_Calls : INITIAL_ZONE;
            }
            void reset()
            {
                m_Success = 0;
                m_Calls   = 0;
            }
            bool empty() const
            {
                return m_Calls == 0;
            }
        };

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        static constexpr unsigned BUCKET_COUNT = 30;

        std::array<Bucket, BUCKET_COUNT> m_Buckets;

        unsigned m_CurrentBucket = 0;
        time_t   m_CurrentTime   = 0;
        time_t   m_RedUntil      = 0; // block mode until this time
        time_t   m_HealUntil     = 0; // heal mode until this time

        enum class Zone
        {
            RED,
            HEAL,
            YELLOW,
            GREEN,
        };
        Zone   m_Zone        = Zone::YELLOW;
        double m_SuccessRate = INITIAL_ZONE;

        Duration m_Duration;

        Zone calc_zone() const
        {
            if (m_RedUntil > 0)
                return Zone::RED;
            else if (m_HealUntil > 0)
                return Zone::HEAL;
            else if (m_SuccessRate > YELLOW_ZONE)
                return Zone::GREEN;
            return Zone::YELLOW;
        }

        void switch_zone()
        {
            // zone transitions
            if (m_RedUntil == 0 and m_HealUntil == 0 and m_SuccessRate <= RED_ZONE) {
                m_RedUntil = m_CurrentTime + DELAY;
            } else if (m_RedUntil > 0 and m_RedUntil <= m_CurrentTime) {
                m_RedUntil  = 0;
                m_HealUntil = m_CurrentTime + DELAY;
            } else if (m_HealUntil > 0 and m_HealUntil <= m_CurrentTime) {
                m_HealUntil = 0;
            }
            m_Zone = calc_zone();

            // account next second
            switch (m_Zone) {
            case Zone::RED: m_Duration.red++; break;
            case Zone::HEAL: m_Duration.heal++; break;
            case Zone::YELLOW: m_Duration.yellow++; break;
            case Zone::GREEN:
            default:
                m_Duration.green++;
            }
        }

        double calc_success_rate() const
        {
            double sSum = 0;
            for (auto& x : m_Buckets)
                sSum += x.get();
            return sSum / m_Buckets.size();
        }

        void prepare(time_t aNow)
        {
            if (aNow <= m_CurrentTime)
                return;

            m_CurrentTime = aNow;

            if (!m_Buckets[m_CurrentBucket].empty()) {
                m_CurrentBucket += 1;
                m_CurrentBucket = m_CurrentBucket % m_Buckets.size();
                m_SuccessRate   = calc_success_rate();
                m_Buckets[m_CurrentBucket].reset();
            }
            switch_zone();
        }

    public:
        static constexpr double INITIAL_ZONE = 0.75;
        static constexpr double RED_ZONE     = 0.50;
        static constexpr double HEAL_ZONE    = 0.10; // rate in heal mode
        static constexpr double YELLOW_ZONE  = 0.95;
        static constexpr time_t DELAY        = 10; // delay before healing, duration of heal/block mode.

        void insert(time_t aNow, bool aSuccess)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);
            m_Buckets[m_CurrentBucket].insert(aSuccess);
        }

        bool test()
        {
            Lock sLock(m_Mutex);

            switch (m_Zone) {
            case Zone::RED: return false;
            case Zone::HEAL: return Util::drand48() < HEAL_ZONE;
            case Zone::YELLOW: return Util::drand48() < m_SuccessRate;
            case Zone::GREEN:
            default:
                return true;
            }
        }

        void timer(time_t aNow)
        {
            Lock sLock(m_Mutex);
            prepare(aNow);
        }

        double success_rate() const
        {
            Lock sLock(m_Mutex);
            return m_SuccessRate;
        }

        Duration duration() const
        {
            Lock sLock(m_Mutex);
            return m_Duration;
        }
    };

    class Breaker : public std::enable_shared_from_this<Breaker>
    {
        boost::asio::io_service&  m_Service;
        std::atomic<bool>         m_Stop{false};
        boost::asio::steady_timer m_Timer;

        mutable std::mutex m_Mutex;
        using Lock = std::unique_lock<std::mutex>;
        std::map<std::string, std::shared_ptr<PeerState>> m_State;

        void timer(boost::asio::yield_context yield)
        {
            const time_t sNow = time(nullptr);
            Lock         sLock(m_Mutex);
            for (auto& x : m_State)
                x.second->timer(sNow);
        }

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

    public:
        using Error = Exception::Error<Breaker>;

        Breaker(boost::asio::io_service& aService)
        : m_Service(aService)
        , m_Timer(aService)
        {
        }

        void start()
        {
            boost::asio::spawn(m_Timer.get_executor(), [this, p = shared_from_this()](boost::asio::yield_context yield) {
                boost::beast::error_code ec;
                while (!m_Stop) {
                    timer(yield);
                    m_Timer.expires_from_now(std::chrono::seconds(1));
                    m_Timer.async_wait(yield[ec]);
                }
            });
        }

        void stop()
        {
            m_Stop = true;
            m_Timer.cancel();
        }

        template <class T>
        asio_http::Response wrap(const std::string& aKey, T&& aHandler)
        {
            auto sState = getOrCreate(aKey);
            if (!sState->test())
                throw Error("request to " + aKey + " blocked by circuit breaker");
            try {
                asio_http::Response sResponse = aHandler();
                sState->insert(time(nullptr), goodResponse(sResponse));
                return sResponse;
            } catch (...) {
                sState->insert(time(nullptr), false);
                throw;
            }
        }

        double success_rate(const std::string& aKey)
        {
            return getOrCreate(aKey)->success_rate();
        }
    };
} // namespace SD