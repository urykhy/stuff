#pragma once

#include <xxhash.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Bloom {
    using SmallKey = std::array<uint32_t, 4>;

    // n - number of stored elements
    // m - number of bits
    // k - number of bits per element (number hashes per element)
    inline double estimate(double n, double m, double k)
    {
        const double x = k * n / m;
        return std::pow(1 - std::exp(-x), k);
    }

    namespace {
        inline XXH128_hash_t xxh(std::string_view aInput)
        {
            return XXH3_128bits(aInput.data(), aInput.size());
        }

        template <class T>
        typename std::enable_if<std::is_integral_v<T>, XXH128_hash_t>::type xxh(const T& aInput)
        {
            return XXH3_128bits(&aInput, sizeof(T));
        }
    } // namespace

    template <class T>
    inline SmallKey hash(const T& aVal)
    {
        SmallKey      sKey;
        XXH128_hash_t sHash = xxh(aVal);
        static_assert(sizeof(sKey) == sizeof(sHash));
        memcpy(&sKey, &sHash, sizeof(sHash));
        return sKey;
    }

    class Set
    {
        const unsigned       m_Size;
        std::vector<uint8_t> m_Data;

        std::pair<unsigned, uint8_t> prepare(uint32_t aBit) const
        {
            return std::make_pair(aBit >> 3, 1 << (aBit & 0x07));
        }

        void set(uint32_t aKey)
        {
            auto [aIndex, aByte] = prepare(aKey);
            m_Data[aIndex] |= aByte;
        }

        bool get(uint32_t aKey) const
        {
            auto [aIndex, aByte] = prepare(aKey);
            return (m_Data[aIndex] & aByte) == aByte;
        }

    public:
        Set(unsigned aBits)
        : m_Size(aBits)
        , m_Data(aBits / 8)
        {
            assert(aBits % 8 == 0);
        }

        template <class T, std::size_t N>
        void insert(const std::array<T, N>& aSet)
        {
            for (auto& x : aSet)
                set(x % m_Size);
        }

        template <class T, std::size_t N>
        bool test(const std::array<T, N>& aSet) const
        {
            for (auto& x : aSet)
                if (!get(x % m_Size))
                    return false;
            return true;
        }

        void clear()
        {
            std::fill(m_Data.begin(), m_Data.end(), 0);
        }
    };

    class Filter
    {
        Set  m_Set1;
        Set  m_Set2;
        bool m_Actual1 = true;

        unsigned       m_Counter = 0;
        const unsigned m_Rotate;

        void Rotate()
        {
            auto& sActual = m_Actual1 ? m_Set1 : m_Set2;
            sActual.clear();
            m_Actual1 = !m_Actual1;
            m_Counter = 0;
        }

    public:
        Filter(unsigned aBits = 8 * 20 * 1024, unsigned aRotate = 5 * 1024)
        : m_Set1(aBits)
        , m_Set2(aBits)
        , m_Rotate(aRotate)
        {}

        template <class T>
        void Put(const T& aKey)
        {
            m_Set1.insert(aKey);
            m_Set2.insert(aKey);

            m_Counter++;
            if (m_Counter > m_Rotate)
                Rotate();
        }

        template <class T>
        bool Check(const T& aKey)
        {
            auto& sActual = m_Actual1 ? m_Set1 : m_Set2;
            return sActual.test(aKey);
        }
    };
} // namespace Bloom
