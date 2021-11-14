#pragma once

#include "GetOrCreate.hpp"

namespace Prometheus {
    class Notice
    {
        using C = Counter<uint8_t>;
        GetOrCreate       m_Store;
        const std::string m_Name;

    public:
        Notice(const std::string& aName = "status")
        : m_Name(aName)
        {}

        struct Key
        {
            std::string priority = "notice";
            std::string message;

            std::string tag() const
            {
                return "{priority=\"" + priority + "\",message=\"" + message + "\"}";
            }
        };

        void set(const Key& aKey)
        {
            m_Store.get<C>(m_Name + aKey.tag())->set(1);
        }
        void clear(const Key& aKey)
        {
            m_Store.get<C>(m_Name + aKey.tag())->set(0);
        }
    };
} // namespace Prometheus