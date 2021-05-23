#pragma once

#include <array>
#include <cmath>

#include <unsorted/Random.hpp>

namespace Prometheus {
    class Histogramm
    {
        size_t              m_Size;
        std::vector<double> m_Storage;
        double              m_Min{};
        double              m_Max{};
        bool                m_HasMin{false};

    public:
        Histogramm(const size_t aSize = 1000)
        : m_Size(aSize)
        {
            m_Storage.reserve(aSize);
        }

        void tick(double aValue)
        {
            if (m_HasMin)
                m_Min = std::min(m_Min, aValue);
            else {
                m_Min    = aValue;
                m_HasMin = true;
            }
            m_Max = std::max(m_Max, aValue);

            if (m_Storage.size() < m_Size)
                m_Storage.push_back(aValue);
            else
                m_Storage[Util::randomInt(m_Size)] = aValue;
        }

        template <class P>
        auto quantile(const P& aParam) -> P
        {
            P    sResult{};
            std::sort(m_Storage.begin(), m_Storage.end());

            if (m_Storage.size() == 0)
                return sResult;
            const auto sSize = m_Storage.size() - 1;
            for (unsigned i = 0; i < aParam.size(); i++) {
                auto sPhi = aParam[i];
                if (sPhi <= 0)
                    sResult[i] = m_Min;
                else if (sPhi >= 1)
                    sResult[i] = m_Max;
                else {
                    auto sIdx  = std::min(size_t(std::round(sPhi * sSize)), sSize);
                    sResult[i] = m_Storage[sIdx];
                }
            }
            return sResult;
        }

        void clear()
        {
            m_Storage.clear();
            m_Storage.reserve(m_Size);
            m_Min     = 0;
            m_Max     = 0;
            m_HasMin  = false;
        }
    };
} // namespace Prometheus
