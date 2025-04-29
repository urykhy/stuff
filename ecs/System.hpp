#pragma once

#include "Component.hpp"

namespace ECS {

    template <class... V>
    class Entity;

    template <class... V>
    struct TmpEntity;

    template <class... V>
    class System : protected Store<V...>
    {
        friend class Entity<V...>;
        uint64_t m_Serial{1}; // id 0 reserved. Entity d-tor uses it.

        uint64_t nextEntityID() { return m_Serial++; }

    public:
        template <class C, class F>
        void forEach(F&& aFunc)
        {
            for (auto& x : this->template get<C>().m_Store)
                aFunc(TmpEntity<V...>(x.first), x.second);
        }
    };

} // namespace ECS
