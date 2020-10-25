#pragma once

#include <functional>
#include <list>
#include <memory>
#include <optional>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "Algorithm.hpp"

namespace Container {

    // hack to create namespace alias
    namespace {
        namespace mi = boost::multi_index;
    }

    // T must be shared_ptr
    template <class T>
    class Pool
    {
        struct Rec
        {
            T         data;
            uintptr_t serial;
            time_t    deadline;

            struct _key
            {};
            struct _deadline
            {};

            Rec()
            : serial(0)
            , deadline(0)
            {}

            Rec(T aPtr)
            : data(aPtr)
            , serial(reinterpret_cast<uintptr_t>(aPtr.get()))
            , deadline(::time(nullptr))
            {}
        };

        using Store = boost::multi_index_container<
            Rec,
            mi::indexed_by<
                mi::ordered_unique<
                    mi::tag<typename Rec::_key>, mi::member<Rec, uintptr_t, &Rec::serial>>,
                mi::ordered_non_unique<
                    mi::tag<typename Rec::_deadline>, mi::member<Rec, time_t, &Rec::deadline>>>>;

        Store m_Store;

    public:
        std::optional<T> get()
        {
            if (m_Store.empty())
                return {};

            auto& sStore = mi::get<typename Rec::_key>(m_Store);
            auto  sKeyIt = sStore.begin();
            auto  sPtr   = sKeyIt->data;
            sStore.erase(sKeyIt);
            return sPtr;
        }

        void insert(T& p)
        {
            m_Store.insert(p);
            p.reset();
        }

        void cleanup(const time_t aTimeout = 10)
        {
            const time_t sNow   = ::time(nullptr);
            auto&        sStore = mi::get<typename Rec::_deadline>(m_Store);
            while (!sStore.empty()) {
                auto sIt = sStore.begin();
                if (sIt->deadline + aTimeout < sNow)
                    sStore.erase(sIt);
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
            p.reset();
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
