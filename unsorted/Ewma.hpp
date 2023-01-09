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

    class EwmaTs
    {
        Ewma     m_Ewma;
        time_t   m_LastUpdateTs = 0;
        double   m_Value        = 0;
        uint32_t m_Count        = 0;

    public:
        EwmaTs(double aFactor = 0.95, double aValue = 0)
        : m_Ewma(aFactor, aValue)
        {
        }

        bool add(double aValue, time_t aNow)
        {
            bool sNewSecond = false;
            if (aNow > m_LastUpdateTs) {
                if (m_Count > 0) {
                    m_Ewma.add(m_Value / (double)m_Count);
                }
                m_Value        = 0;
                m_Count        = 0;
                m_LastUpdateTs = aNow;
                sNewSecond     = true;
            };
            m_Value += aValue;
            m_Count++;
            return sNewSecond;
        }
        double estimate() const { return m_Ewma.estimate(); }
        void   reset(double aValue) { m_Ewma.reset(aValue); }
    };
} // namespace Util
