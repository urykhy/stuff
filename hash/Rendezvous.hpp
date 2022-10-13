#pragma once

#include <xxhash.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Hash {
    class Rendezvous
    {
        struct Node
        {
            std::string name = "";
            uint64_t    seed = 0;
        };
        std::vector<Node> m_Nodes;

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
        using ServerList = std::vector<std::string>;

        Rendezvous(const ServerList& aList)
        {
            for (const auto& x : aList)
                m_Nodes.push_back(Node{.name = x, .seed = hash(x)});
        }

        template <class T>
        std::string_view operator()(T&& aKey) const
        {
            uint64_t sMax   = 0;
            uint32_t sIndex = 0;
            for (uint32_t i = 0; i < m_Nodes.size(); i++) {
                auto sTmp = hash(aKey, m_Nodes[i].seed);
                if (sTmp > sMax) {
                    sIndex = i;
                    sMax   = sTmp;
                }
            }
            return m_Nodes[sIndex].name;
        }
    };
} // namespace Hash