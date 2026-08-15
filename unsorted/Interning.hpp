#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Util {
    class Interning
    {
        uint32_t                                  m_Serial{0};
        std::unordered_map<std::string, uint32_t> m_Map;

    public:
        uint32_t operator()(const std::string& aStr)
        {
            auto sIt = m_Map.find(aStr);
            if (sIt == m_Map.end()) {
                auto [sInsert, _] = m_Map.emplace(aStr, m_Serial++);
                sIt               = sInsert;
            }
            return sIt->second;
        }
        uint32_t size() const
        {
            return m_Serial;
        }
    };
} // namespace Util
