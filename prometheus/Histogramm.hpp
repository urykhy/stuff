#pragma once

#include <array>
#include <cmath>

#include <unsorted/Random.hpp>

namespace Prometheus {
    class Histogramm
    {
        size_t              m_Size;
        size_t              m_Count = 0;
        std::vector<double> m_Storage;
        double              m_Min{};
        double              m_Max{};

    public:
        Histogramm(const size_t aSize = 1000)
        : m_Size(aSize)
        {
            m_Storage.reserve(aSize);
        }

        void tick(double aValue)
        {
            if (m_Storage.empty()) {
                m_Min = aValue;
                m_Max = aValue;
            } else {
                m_Min = std::min(m_Min, aValue);
                m_Max = std::max(m_Max, aValue);
            }

            if (m_Count < m_Size)
                m_Storage.push_back(aValue);
            else {
                // https://en.wikipedia.org/wiki/Reservoir_sampling
                const size_t sPos = Util::randomInt(m_Count);
                if (sPos < m_Size)
                    m_Storage[sPos] = aValue;
            }
            m_Count++;
        }

        template <class P>
        auto quantile(const P& aParam) -> P
        {
            P sResult{};
            std::sort(m_Storage.begin(), m_Storage.end());

            if (m_Storage.size() == 0)
                return sResult;
            const auto sMaxIdx = m_Storage.size() - 1;
            for (unsigned i = 0; i < aParam.size(); i++) {
                auto sPhi = aParam[i];
                if (sPhi <= 0)
                    sResult[i] = m_Min;
                else if (sPhi >= 1)
                    sResult[i] = m_Max;
                else {
                    const auto sIdx = std::min(size_t(std::round(sPhi * sMaxIdx)), sMaxIdx);
                    if (m_Storage.size() % 2 == 0 and sIdx >= 1) {
                        sResult[i] = (m_Storage[sIdx] + m_Storage[sIdx - 1]) / 2;
                    } else {
                        sResult[i] = m_Storage[sIdx];
                    }
                }
            }
            return sResult;
        }

        void clear()
        {
            m_Count = 0;
            m_Storage.clear();
            m_Storage.reserve(m_Size);
            m_Min = 0;
            m_Max = 0;
        }
    };
} // namespace Prometheus
