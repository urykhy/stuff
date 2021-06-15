#pragma once
#include <boost/core/noncopyable.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>

#include <bloom/Bloom.hpp>

namespace Cache {

    namespace {
        using namespace boost::multi_index;
    }

    template <class Key, class Value>
    class LFU : public boost::noncopyable
    {
    protected:
        struct Entry
        {
            Key      key;
            Value    value;
            uint64_t freq;

            struct _key
            {};
            struct _freq
            {};
        };

        using Store = boost::multi_index_container<
            Entry,
            indexed_by<hashed_unique<
                           tag<typename Entry::_key>, member<Entry, Key, &Entry::key>>,
                       ordered_non_unique<
                           tag<typename Entry::_freq>, member<Entry, uint64_t, &Entry::freq>>>>;
        Store          m_Store;
        uint64_t       m_Clock = 0;
        const size_t   m_MaxSize;
        const unsigned m_Cost;

        void Shrink()
        {
            if (m_Store.size() <= m_MaxSize)
                return;
            auto& fs = get<typename Entry::_freq>(m_Store);
            m_Clock  = fs.begin()->freq;
            fs.erase(fs.begin());
        }

    public:
        explicit LFU(size_t aSize, unsigned aCost = 20)
        : m_MaxSize(aSize)
        , m_Cost(aCost)
        {}

        const Value* Get(const Key& aKey)
        {
            auto& ks = get<typename Entry::_key>(m_Store);
            auto  i  = ks.find(aKey);
            if (i == ks.end())
                return nullptr;
            m_Store.modify(i, [this](auto& x) { x.freq += m_Cost; });
            return &i->value;
        }

        void Put(const Key& aKey, const Value& aValue, unsigned aCost = 1)
        {
            auto& ks = get<typename Entry::_key>(m_Store);
            auto  i  = ks.find(aKey);
            if (i != ks.end()) {
                m_Store.modify(i, [&aValue, this](auto& x) { x.value = aValue; x.freq += m_Cost; });
            } else {
                m_Store.insert(Entry{aKey, aValue, m_Clock + aCost});
                Shrink();
            }
        }

        size_t Size() const
        {
            return m_Store.size();
        }

#ifdef BOOST_TEST_MESSAGE
        template <class T>
        void debug(T&& t) const
        {
            for (auto& x : m_Store)
                t(x);
        }
#endif
    };

    // BloomFilter in front of LFU
    template <class Key, class Value>
    class BF_LFU : public LFU<Key, Value>
    {
        using Parent = LFU<Key, Value>;
        Bloom::Filter m_Bloom;

    public:
        BF_LFU(size_t aSize, unsigned aCost = 20, unsigned aBloomBits = 8 * 512 * 1024, unsigned aRotate = 128 * 1024)
        : Parent(aSize, aCost)
        , m_Bloom(aBloomBits, aRotate)
        {}

        void Put(const Key& aKey, const Value& aValue)
        {
            unsigned   sCost = 1;
            const auto sHash = Bloom::hash(aKey);
            if (!m_Bloom.Check(sHash)) {
                m_Bloom.Put(sHash);
                if (Parent::Size() >= Parent::m_MaxSize)
                    return;
            } else {
                sCost += Parent::m_Cost; // boost cost, since we already seen this key
            }
            Parent::Put(aKey, aValue, sCost);
        }
    };

} // namespace Cache
