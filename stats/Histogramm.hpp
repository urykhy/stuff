#pragma once

#include <array>
#include <cmath>

namespace Stat
{
    template<unsigned SIZE = 1000>
    class Histogramm
    {
        using A = std::array<uint64_t, SIZE>;
        A m_Storage{};
        const float m_Max;

    public:
        Histogramm(const float aMax = 10)
        : m_Max(aMax)
        {}

        unsigned bucket(float v) const
        {
            unsigned sV = std::round(v * SIZE / m_Max);
            return std::clamp(sV, 0u, SIZE - 1);
        }

        void tick(unsigned aBucket)
        {
            m_Storage[aBucket]++;
        }

        size_t total() const
        {
            size_t sCount = 0;
            for (auto& x : m_Storage)
                sCount += x;
            return sCount;
        }

        template<class P, class T>
        void quantile(const P& aProb, T aHandler) const
        {
            const float sTotal = total();
            size_t sIndex = 0;
            float  sCount = 0;
            unsigned sMaxUsedBucket = 0;

            for (unsigned i = 0; i < SIZE; i++)
            {
                sCount += m_Storage[i];
                if (m_Storage[i] > 0)
                    sMaxUsedBucket = i;

                if (sCount / sTotal > aProb[sIndex])
                {
                    if (sIndex >= aProb.size())
                        continue;
                    aHandler(sIndex, m_Max * i / SIZE);
                    sIndex++;
                }
            }
            aHandler(sIndex, m_Max * sMaxUsedBucket / SIZE); // max value
        }

        void clear()
        {
            m_Storage = {};
        }
    };
}
