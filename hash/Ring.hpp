#pragma once

#include <xxhash.h>

#include <string>
#include <vector>

#include <boost/container/static_vector.hpp>

namespace Hash {
    class Ring
    {
        struct Info
        {
            uint64_t hash = 0;
            uint32_t id   = 0;
            uint32_t dc   = 0;
        };
        std::vector<Info> m_Map;

        uint64_t hash(const std::string_view aName, uint64_t aSeed = 0) const
        {
            return XXH3_64bits_withSeed(aName.data(), aName.size(), aSeed);
        }

        template <class T>
        typename std::enable_if<std::is_integral_v<T>, uint64_t>::type
        hash(const T aValue, uint64_t aSeed = 0) const
        {
            return XXH3_64bits_withSeed(&aValue, sizeof(aValue), aSeed);
        }

    public:
        Ring()
        {
        }

        void insert(const std::string& aName, uint32_t aID, uint32_t aDC, double aWeight)
        {
            const uint64_t sHash = hash(aName);
            for (auto i = 0; i < aWeight; i++)
                m_Map.push_back(Info{hash(i, sHash), aID, aDC});
        }

        void prepare()
        {
            std::sort(m_Map.begin(), m_Map.end(), [](const auto& a, const auto& b) { return a.hash < b.hash; });
        }

        using IDS = boost::container::static_vector<uint32_t, 3>;

        IDS operator()(const uint64_t aHash) const
        {
            constexpr unsigned STEPS = 15;
            IDS                sIDS;
            IDS                sDC;
            auto               sIt = std::lower_bound(m_Map.begin(), m_Map.end(), aHash, [](const auto& a, uint64_t aVal) { return a.hash < aVal; });

            for (unsigned i = 0; sIDS.size() < sIDS.capacity() && i < STEPS; i++, sIt++) {
                if (sIt == m_Map.end())
                    sIt = m_Map.begin();
                if (std::find(sDC.begin(), sDC.end(), sIt->dc) != sDC.end())
                    continue;
                if (std::find(sIDS.begin(), sIDS.end(), sIt->id) != sIDS.end())
                    continue;
                sDC.push_back(sIt->dc);
                sIDS.push_back(sIt->id);
            }
            return sIDS;
        }
    };
} // namespace Hash