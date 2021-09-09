#pragma once

#include <functional>
#include <list>
#include <memory>
#include <optional>

#include "Algorithm.hpp"

namespace Container {

    // T must be shared_ptr
    template <class T>
    class Pool
    {
        struct Data
        {
            time_t timestamp;
            T      data;
        };
        std::list<Data> m_Store;

    public:
        std::optional<T> get()
        {
            if (m_Store.empty())
                return {};

            auto sPtr = m_Store.back().data;
            m_Store.pop_back();
            return sPtr;
        }

        void insert(T& p)
        {
            m_Store.push_back({time(nullptr), p});
            p.reset();
        }

        void cleanup(time_t aTimeout = 10)
        {
            const time_t sNow = time(nullptr);
            while (!m_Store.empty()) {
                if (m_Store.front().timestamp + aTimeout < sNow)
                    m_Store.pop_front();
                else
                    break;
            }
        }

        size_t size() const { return m_Store.size(); }
        bool   empty() const { return m_Store.empty(); }
    };

    template <class Key, class T>
    class KeyPool
    {
        using Set = std::map<Key, Pool<T>>;
        Set m_Set;

    public:
        std::optional<T> get(const Key& aKey)
        {
            auto sIt = m_Set.find(aKey);
            if (sIt == m_Set.end())
                return {};
            return sIt->second.get();
        }

        void insert(const Key& aKey, T& p)
        {
            m_Set[aKey].insert(p);
        }

        void cleanup(const time_t aTimeout = 10)
        {
            for (auto& [sKey, sData] : m_Set)
                sData.cleanup(aTimeout);
            discard_if(m_Set, [](auto& s) -> bool {
                return s.second.empty() == 0;
            });
        }

        size_t size() const
        {
            size_t sSize = 0;
            for (auto& [sKey, sPool] : m_Set)
                sSize += sPool.size();
            return sSize;
        }

        bool empty() const { return m_Set.empty(); }
    };

    template <class T>
    class ProducePool : public Pool<T>
    {
        using Create = std::function<T()>;
        using Check  = std::function<bool(T)>;
        Create m_Create;
        Check  m_Check;

    public:
        ProducePool(Create aCreate, Check aCheck)
        : m_Create(aCreate)
        , m_Check(aCheck)
        {}

        T get()
        {
            auto sItem = Pool<T>::get();
            if (sItem and m_Check(sItem.value()))
                return sItem.value();
            return m_Create();
        }
    };

} // namespace Container
