#pragma once
#include <list>
#include <map>

namespace Cache {

    template<class Key, class Value>
    class LRU {
    private:
        LRU(const LRU&) = delete;
        LRU& operator=(const LRU&) = delete;

        typedef std::list<std::pair<Key,Value>> TList;
        typedef std::map<Key, typename TList::iterator> TMap;
        typedef std::size_t size_t;

        TList m_Lru;
        TMap m_Index;
        const size_t m_MaxSize;

        void Remove(const Key& key)
        {
            auto i = m_Index.find(key);
            m_Lru.erase(i->second);
            m_Index.erase(i);
        }

        void Update(const typename TMap::iterator& i)
        {
            m_Lru.splice(m_Lru.begin(), m_Lru, i->second);
            i->second = m_Lru.begin();
        }

        void Insert(const Key& key, const Value& value)
        {
            m_Lru.push_front(std::make_pair(key,value));
            auto i = m_Lru.begin();
            m_Index.insert(std::make_pair(key,i));
        }

        void Shrink()
        {
            if (m_Lru.size() > m_MaxSize)
            {
                auto i = m_Lru.end();
                --i;
                Remove(i->first);
            }
        }

    public:
        explicit LRU(size_t mSize)
        : m_MaxSize(mSize)
        {
            ;;
        }

        const Value* Get(const Key& key)
        {
            auto i = m_Index.find(key);
            if (i != m_Index.end())
            {
                Update(i);
                return &i->second->second;
            }
            return nullptr;
        }

        void Put(const Key& key, const Value& value)
        {
            auto i = m_Index.find(key);
            if (i != m_Index.end())
            {
                Update(i);
                i->second->second = value;
            } else {
                Insert(key, value);
                Shrink();
            }
        }
    };
}
