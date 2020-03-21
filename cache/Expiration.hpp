#pragma once
#include <time.h>
#include <cassert>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace Cache
{
    using namespace boost::multi_index;

    template<class K, class V>
    class Expiration
    {
        struct Rec {
            K key;
            V value;
            time_t ts;

            // tags
            struct _key {};
            struct _ts {};

            Rec() : ts(0) {}
            Rec(const K& k, const V& v) : key(k), value(v), ts(time(0)) { }
        };

        using Store = boost::multi_index_container<Rec,
            indexed_by<
                hashed_unique<
                    tag<typename Rec::_key>, member<Rec, K, &Rec::key>
                >,
                ordered_non_unique<
                    tag<typename Rec::_ts>, member<Rec, time_t, &Rec::ts>
                >
            >
        >;
        Store m_Store;
        const size_t m_Max;
        const time_t m_Expiration;

    public:
        Expiration(size_t m, time_t ex) : m_Max(m), m_Expiration(ex) {}

        const V* Get(const K& k) const
        {
            auto& ks = get<typename Rec::_key>(m_Store);
            auto it = ks.find(k);
            if ( it != ks.end() )
            {
                if (it->ts + m_Expiration > time(0))
                    return &(it->value);
            }
            return nullptr;
        }

        void Put(const K& key, const V& value)
        {
            if (size() + 1 > m_Max)
            {
                auto& ks = get<typename Rec::_ts>(m_Store);
                auto it = ks.begin();
                ks.erase(it);
            }
            m_Store.insert(Rec(key, value));
        }

        bool empty()  const { return m_Store.empty(); }
        size_t size() const { return m_Store.size(); }
    };
}
