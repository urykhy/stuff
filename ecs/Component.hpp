#pragma once

#include <map>
#include <optional>

namespace ECS {

    // not thread safe
    template <class V>
    struct ComponentStore
    {
        std::map<uint64_t, V> m_Store; // entity_id to component

        void create(uint64_t aEntity, V&& aData)
        {
            m_Store.emplace(aEntity, std::move(aData));
        }

        void erase(uint64_t aEntity)
        {
            m_Store.erase(aEntity);
        }

        std::optional<V*> get(uint64_t aEntity)
        {
            auto sIt = m_Store.find(aEntity);
            if (sIt == m_Store.end())
                return std::nullopt;
            return &sIt->second;
        }

        template <class F>
        void inspect(uint64_t aEntity, F&& aFunc)
        {
            auto sIt = m_Store.find(aEntity);
            if (sIt == m_Store.end())
                return;
            aFunc(sIt->second);
        }
    };

    template <class... V>
    struct Store
    {
        template <class T>
        ComponentStore<T>& get() { return std::get<ComponentStore<T>>(m_Store); }

        std::tuple<ComponentStore<V>...> m_Store;
    };

} // namespace ECS
