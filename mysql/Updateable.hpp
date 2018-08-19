#pragma once

#include <Client.hpp>
#include <experimental/optional>

namespace MySQL
{
    template<class Policy>
    class Updateable
    {
        typename Policy::Container m_Data;
        using Key   = typename Policy::Container::key_type;
        using Value = typename Policy::Container::mapped_type;
    public:
        Updateable() {}
        void update(MySQL::Connection& aConnection)
        {
            typename Policy::Container sData;
            aConnection.Query(Policy::query());
            aConnection.Use([&sData](const MySQL::Row& aRow) { Policy::parse(sData, aRow); });
            std::swap(m_Data, sData);
        }
        std::experimental::optional<Value> find(const Key& k) const
        {
            auto sIter = m_Data.find(k);
            if (sIter == m_Data.end())
                return std::experimental::nullopt;
            return std::experimental::make_optional<Value>(Value{sIter->second});
        }
        size_t size()  const { return m_Data.size(); }
        bool   empty() const { return m_Data.empty(); }
    };
}
