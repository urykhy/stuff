#pragma once

namespace Util {
    class Ewma
    {
        double m_Factor;
        double m_Value;

    public:
        explicit Ewma(double aFactor = 0.95, double aValue = 0)
        : m_Factor(aFactor)
        , m_Value(aValue)
        {
        }

        void   add(double aValue) { m_Value = m_Value * m_Factor + (1 - m_Factor) * aValue; }
        double estimate() const { return m_Value; }
        void   reset(double aValue) { m_Value = aValue; }
    };

    class EwmaRps
    {
        Ewma m_Latency;
        Ewma m_RPS;
        Ewma m_SuccessRate;

        time_t   m_LastUpdateTs = 0;
        double   m_Elapsed      = 0;
        uint32_t m_Count        = 0;
        uint32_t m_Success      = 0;

    public:
        struct Info
        {
            double latency      = 0;
            double rps          = 0;
            double success_rate = 0;
        };

        EwmaRps(double aFactor = 0.95, double aLatency = 0, double aSuccessRate = 1.0)
        : m_Latency(aFactor, aLatency)
        , m_RPS(aFactor, 0)
        , m_SuccessRate(aFactor, aSuccessRate)
        {
        }

        bool add(double aValue, time_t aNow, bool aSuccess)
        {
            bool sNewSecond = false;
            if (aNow > m_LastUpdateTs) {
                if (m_Count > 0) {
                    double sTime = aNow - m_LastUpdateTs;
                    m_Latency.add(m_Elapsed / (double)m_Count);
                    m_RPS.add(m_Count / sTime);
                    m_SuccessRate.add(m_Success / (double)m_Count);
                }
                m_Elapsed      = 0;
                m_Count        = 0;
                m_Success      = 0;
                m_LastUpdateTs = aNow;
                sNewSecond     = true;
            };
            m_Elapsed += aValue;
            m_Count++;
            if (aSuccess)
                m_Success++;
            return sNewSecond;
        }
        Info estimate() const
        {
            return {m_Latency.estimate(), m_RPS.estimate(), m_SuccessRate.estimate()};
        }
        void reset(const Info& aInfo)
        {
            m_Latency.reset(aInfo.latency);
            m_RPS.reset(aInfo.rps);
            m_SuccessRate.reset(aInfo.success_rate);
        }
    };
} // namespace Util
