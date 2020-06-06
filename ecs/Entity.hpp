#pragma once

#include <mpl/Mpl.hpp>

#include "System.hpp"

namespace ECS {

    template <class... V>
    class Entity
    {
    protected:
        using S = System<V...>;
        uint64_t m_ID;

        Entity() = delete;

        Entity(const Entity& aOther) = delete;
        Entity& operator=(const Entity& aOther) = delete;

        Entity(uint64_t aID)
        : m_ID(aID)
        {}

    public:
        Entity(Entity&& aOther)
        : m_ID(aOther.m_ID)
        {
            aOther.m_ID = 0;
        }

        ~Entity()
        {
            if (m_ID > 0)
                Mpl::for_each_element([this](auto& x) { x.erase(m_ID); }, system().m_Store);
        }

        uint64_t getID() const { return m_ID; }

        static Entity create()
        {
            return Entity<V...>(system().nextEntityID());
        }

        template <class C>
        void assign(C&& aComponent)
        {
            system().template get<C>().create(m_ID, std::move(aComponent));
        }

        template <class C>
        void erase()
        {
            system().template get<C>().erase(m_ID);
        }

        template <class C>
        std::optional<C*> get()
        {
            return system().template get<C>().get(m_ID);
        }

        template <class F>
        void inspect(F&& aFunc)
        {
            Mpl::for_each_element([this, aFunc = std::forward<F>(aFunc)](auto& x) mutable { x.inspect(m_ID, aFunc); }, system().m_Store);
        }

        static S& system()
        {
            static S m_System;
            return m_System;
        }
    };

    template <class... V>
    struct TmpEntity : public Entity<V...>
    {
        TmpEntity(uint64_t aID)
        : Entity<V...>(aID)
        {}

        ~TmpEntity()
        { // erase id, so Entity d-tor will not cleanup
            this->m_ID = 0;
        }
    };

} // namespace ECS
