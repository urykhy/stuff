#pragma once

#include <map>
#ifndef NDEBUG
#include <iostream>
#endif

namespace Cache
{
    // ~ LFU + Aging
    // linear complexity at Shrink

    template<class Key, class Value>
    class LFU
    {
        const size_t m_MaxSize;
        size_t m_Age = 0;

        struct Data
        {
            Value value;
            size_t age = 0;
            size_t count = 0;

            float weight(size_t aAge) const
            {
                return count > 0 ? (aAge - age) / (float)count : (aAge - age)*(aAge - age);
            }
        };
        using Map = std::map<Key, Data>;
        Map m_Index;

        void Shrink()
        {
            if (m_Index.size() > m_MaxSize)
            {
                std::multimap<float, typename Map::iterator> list;
                for (auto x = m_Index.begin(); x != m_Index.end(); x++) // FIXME: full scan
                    list.insert(std::make_pair(x->second.weight(m_Age), x));
                auto be = list.rbegin(); // element with largest weight
                auto& w = be->first;
                auto& d = be->second->second;
                //std::cout << "drop " << d.value << " with weight " << w << std::endl;
                m_Index.erase(be->second);
            }
        }

    public:
        explicit LFU(size_t mSize)
        : m_MaxSize(mSize) { ;; }


        const Value* Get(const Key& key)
        {
            m_Age++;
            auto i = m_Index.find(key);
            if (i == m_Index.end())
                return nullptr;
            i->second.count++;
            return &i->second.value;
        }

        void Put(const Key& key, const Value& value)
        {
            m_Age++;
            auto i = m_Index.find(key);
            if (i != m_Index.end())
            {   // put to existing element
                i->second.value = value;
                i->second.count++;
                return;
            }
            // need insert new element
            m_Index.insert(std::make_pair(key, Data{value, m_Age, 0}));
            // drop one element
            Shrink();
        }
#ifndef NDEBUG
        void Print()
        {
            std::multimap<float, typename Map::iterator> list;
            for (auto x = m_Index.begin(); x != m_Index.end(); x++) // FIXME: full scan
                list.insert(std::make_pair(x->second.weight(m_Age), x));
            for (auto& x : list)
                std::cout << x.first << " / " << x.second->second.value << std::endl;
        }
#endif
    };
}
