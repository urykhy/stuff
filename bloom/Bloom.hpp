#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
#include <ssl/Digest.hpp>

namespace Bloom
{
    using SmallKey = std::array<uint32_t, 4>;

    // n - number of stored elements
    // m - number of bits
    // k - number of bits per element (number hashes per element)
    inline double estimate(double n, double m, double k)
    {
        const double x = k*n/m;
        return std::pow(1 - std::exp(-x), k);
    }

    inline SmallKey hash(const std::string& aStr)
    {
        SmallKey sKey;
        const std::string sHash = SSLxx::Digest(EVP_md5(), aStr);
        assert (sizeof(sKey) == sHash.size());
        memcpy(&sKey, sHash.data(), sHash.size());
        return sKey;
    }

    class Set
    {
        const unsigned m_Size;
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

        template<class T, std::size_t N>
        void insert(const std::array<T, N>& aSet)
        {
            for (auto& x : aSet)
                set(x % m_Size);
        }

        template<class T, std::size_t N>
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

} // namespace Bloom
