#pragma once
#include <list>
#include <map>

#include <boost/core/noncopyable.hpp>

namespace Cache {

    template <class Key, class Value>
    class LRU : public boost::noncopyable
    {
    private:
        using List = std::list<std::pair<Key, Value>>;
        using Map  = std::map<Key, typename List::iterator>;

        List         m_Lru;
        Map          m_Index;
        const size_t m_MaxSize;

        void Remove(const Key& key)
        {
            auto i = m_Index.find(key);
            m_Lru.erase(i->second);
            m_Index.erase(i);
        }

        void Update(const typename Map::iterator& i)
        {
            m_Lru.splice(m_Lru.begin(), m_Lru, i->second);
        }

        void Insert(const Key& key, const Value& value)
        {
            m_Lru.push_front(std::make_pair(key, value));
            m_Index[key] = m_Lru.begin();
        }

        void Shrink()
        {
            if (m_Lru.size() <= m_MaxSize)
                return;
            Remove(m_Lru.rbegin()->first);
        }

    public:
        explicit LRU(size_t aSize)
        : m_MaxSize(aSize)
        {}

        const Value* Get(const Key& key)
        {
            auto i = m_Index.find(key);
            if (i != m_Index.end()) {
                Update(i);
                return &i->second->second;
            }
            return nullptr;
        }

        void Put(const Key& key, const Value& value)
        {
            auto i = m_Index.find(key);
            if (i != m_Index.end()) {
                Update(i);
                i->second->second = value;
            } else {
                Insert(key, value);
                Shrink();
            }
        }
    };
} // namespace Cache
