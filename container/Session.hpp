#pragma once

#include <map>

#include <boost/any.hpp>

// store variables in thread_local map

namespace Container::Session {
    class Store
    {
        std::map<std::string, boost::any> m_Map;

    public:
        template <class T>
        void set(const std::string& aKey, const T& aValue)
        {
            m_Map[aKey] = aValue;
        }

        template <class T>
        std::optional<T> get(const std::string& aKey) const
        {
            auto sIt = m_Map.find(aKey);
            if (sIt == m_Map.end())
                return {};
            return std::optional<T>(boost::any_cast<T>(sIt->second));
        }

        void del(const std::string& aKey)
        {
            m_Map.erase(aKey);
        }

        void clear()
        {
            m_Map.clear();
        }
    };

    inline thread_local Store sStore;

    struct Set
    {
        const std::string m_Key;

        template <class T>
        Set(const std::string& aKey, const T& aValue)
        : m_Key(aKey)
        {
            sStore.set(aKey, aValue);
        }

        ~Set() { sStore.del(m_Key); }
    };

    template <class T>
    std::optional<T> get(const std::string& aKey)
    {
        return sStore.get<T>(aKey);
    }

} // namespace Container::Session