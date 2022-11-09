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

        void   add(double x) { m_Value = m_Value * m_Factor + (1 - m_Factor) * x; }
        double estimate() const { return m_Value; }
        void   reset(double aValue) { m_Value = aValue; }
    };
} // namespace Util
