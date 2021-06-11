#pragma once
#include <list>
#include <map>

#include <boost/core/noncopyable.hpp>

namespace Cache {

    template <class Key, class Value>
    class S_LRU : public boost::noncopyable
    {
    private:
        struct Entry
        {
            Key   key;
            Value value;
            bool  prot = false;
        };

        using List = std::list<Entry>;
        using Map  = std::map<Key, typename List::iterator>;

        List         m_Normal;
        List         m_Protected;
        Map          m_Index;
        const size_t m_MaxSize;

        void Remove(const Key& key)
        {
            auto i = m_Index.find(key);
            if (i->second->prot)
                throw std::logic_error("S_LRU::Remove on protected list");
            m_Normal.erase(i->second);
            m_Index.erase(i);
        }

        void Update(const typename Map::iterator& i)
        {
            if (i->second->prot) {
                // refresh in protected list
                m_Protected.splice(m_Protected.begin(), m_Protected, i->second);
            } else {
                // move from normal to protected list
                m_Protected.splice(m_Protected.begin(), m_Normal, i->second);
                i->second->prot = true;

                // move old element from protected to normal
                if (m_Protected.size() > m_MaxSize / 2) {
                    auto sIter  = --(m_Protected.rbegin().base()); // make iter from reverse iter
                    sIter->prot = false;
                    m_Normal.splice(m_Normal.begin(), m_Protected, sIter);
                }
            }
        }

        void Insert(const Key& key, const Value& value)
        {
            m_Normal.push_front(Entry{key, value, false});
            m_Index[key] = m_Normal.begin();
        }

        void Shrink()
        {
            if (m_Normal.size() <= m_MaxSize / 2)
                return;
            Remove(m_Normal.rbegin()->key);
        }

    public:
        explicit S_LRU(size_t mSize)
        : m_MaxSize(mSize)
        {}

        const Value* Get(const Key& key)
        {
            auto i = m_Index.find(key);
            if (i != m_Index.end()) {
                Update(i);
                return &i->second->value;
            }
            return nullptr;
        }

        void Put(const Key& key, const Value& value)
        {
            auto i = m_Index.find(key);
            if (i != m_Index.end()) {
                Update(i);
                i->second->value = value;
            } else {
                Insert(key, value);
                Shrink();
            }
        }
#ifdef BOOST_TEST_MESSAGE
        template <class T>
        void debug(T&& t) const
        {
            for (auto& x : m_Index)
                t(x.second);
        }
#endif
    };
} // namespace Cache
