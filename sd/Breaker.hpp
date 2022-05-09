#pragma once

#include <array>
#include <memory>

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
            uint64_t m_Success = 0;
            uint64_t m_Calls   = 0;

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
        };

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        static constexpr unsigned BUCKET_COUNT = 30;

        std::array<Bucket, BUCKET_COUNT> m_Buckets;

        unsigned m_CurrentBucket = 0;
        time_t   m_MaxTimestamp  = 0;
        time_t   m_Red           = 0; // block mode until this time
        time_t   m_Heal          = 0; // heal mode until this time

        enum class Zone
        {
            RED,
            HEAL,
            YELLOW,
            GREEN,
        };
        Zone   m_Zone        = Zone::YELLOW;
        double m_SuccessRate = INITIAL_ZONE;

        time_t   m_AccountTimestamp = 0;
        Duration m_Duration;

        Zone zone_i() const
        {
            if (m_Red > 0)
                return Zone::RED;
            else if (m_Heal > 0)
                return Zone::HEAL;
            else if (m_SuccessRate > YELLOW_ZONE)
                return Zone::GREEN;
            return Zone::YELLOW;
        }

        void check_zone(time_t aNow)
        {
            if (aNow <= m_AccountTimestamp)
                return;

            m_AccountTimestamp = aNow;

            // zone transitions
            if (m_Red == 0 and m_Heal == 0 and m_SuccessRate <= RED_ZONE) {
                m_Red = aNow + DELAY;
            } else if (m_Red > 0 and m_Red <= aNow) {
                m_Red  = 0;
                m_Heal = aNow + DELAY;
            } else if (m_Heal > 0 and m_Heal <= aNow) {
                m_Heal = 0;
            }
            m_Zone = zone_i();

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

        double
        success_rate_i() const
        {
            double sSum = 0;
            for (auto& x : m_Buckets)
                sSum += x.get();
            return sSum / m_Buckets.size();
        }

        void prepare_i(time_t aNow)
        {
            if (aNow > m_MaxTimestamp) {
                m_MaxTimestamp = aNow;
                m_CurrentBucket += 1;
                m_CurrentBucket = m_CurrentBucket % m_Buckets.size();
                m_SuccessRate   = success_rate_i();
                m_Buckets[m_CurrentBucket].reset();
                check_zone(aNow);
            }
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
            prepare_i(aNow);
            m_Buckets[m_CurrentBucket].insert(aSuccess);
        }

        bool test(time_t aNow)
        {
            Lock sLock(m_Mutex);

            switch (m_Zone) {
            case Zone::RED: return false;
            case Zone::HEAL: return drand48() < HEAL_ZONE;
            case Zone::YELLOW: return drand48() < m_SuccessRate;
            case Zone::GREEN:
            default:
                return true;
            }
        }

        void timer(time_t aNow)
        {
            Lock sLock(m_Mutex);
            check_zone(aNow);
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
} // namespace SD