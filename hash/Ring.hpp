#pragma once

#include <xxhash.h>

#include <algorithm>
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
            uint32_t rack = 0;
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

        void insert(const std::string& aName, uint32_t aID, uint32_t aRack, uint32_t aWeight)
        {
            const uint64_t sHash = hash(aName);
            for (uint32_t i = 0; i < aWeight; i++)
                m_Map.push_back(Info{hash(i, sHash), aID, aRack});
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
            IDS                sRacks;
            auto               sIt = std::lower_bound(m_Map.begin(), m_Map.end(), aHash, [](const auto& a, uint64_t aVal) { return a.hash < aVal; });

            for (unsigned i = 0; sIDS.size() < sIDS.capacity() && i < STEPS; i++, sIt++) {
                if (sIt == m_Map.end())
                    sIt = m_Map.begin();
                if (std::find(sRacks.begin(), sRacks.end(), sIt->rack) != sRacks.end())
                    continue;
                sRacks.push_back(sIt->rack);
                sIDS.push_back(sIt->id);
            }
            return sIDS;
        }
    };
} // namespace Hash