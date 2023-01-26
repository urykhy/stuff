#pragma once

#include "GetOrCreate.hpp"

namespace Prometheus {
    class Notice : public boost::noncopyable
    {
        using C = Counter<uint8_t>;
        static GetOrCreate m_Store;

    public:
        struct Key
        {
            std::string metric   = "status";
            std::string priority = "notice";
            std::string message;

            std::string operator()() const
            {
                return metric + "{priority=\"" + priority + "\",message=\"" + message + "\"}";
            }
        };

        static void set(const Key& aKey)
        {
            m_Store.get<C>(aKey())->set(1);
        }
        static void reset(const Key& aKey)
        {
            m_Store.erase(aKey());
        }
        static void flag(const std::string& aName, bool aValue = true)
        {
            m_Store.get<C>(aName)->set(aValue);
        }
    };

    inline GetOrCreate Notice::m_Store;

} // namespace Prometheus