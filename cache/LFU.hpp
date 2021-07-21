#pragma once
#include <array>
#include <list>
#include <unordered_map>

#include <bloom/Bloom.hpp>

namespace Cache {

    template <class Key, class Value, unsigned Size>
    struct RingAge
    {
        struct Entry
        {
            Key      key;
            Value    value;
            unsigned bucket = 0;
        };
        using List = std::list<Entry>;

    private:
        std::array<List, Size> m_Ring;
        unsigned               m_Clock = 0;

        unsigned boundInc(unsigned aPos, unsigned aW)
        {
            if (aPos < m_Clock)
                aPos += Size;
            unsigned sOffset = aPos - m_Clock;
            sOffset          = std::min(sOffset + aW, Size - 1);
            return (m_Clock + sOffset) % Size;
        }

    public:
        template <class T>
        void Shrink(T&& aHandler)
        {
            while (true) {
                auto& sList = m_Ring[m_Clock];
                if (sList.empty()) {
                    m_Clock = (m_Clock + 1) % Size;
                    continue;
                }
                auto& sItem = sList.front();
                aHandler(sItem);
                sList.pop_front();
                break;
            }
        }

        void Refresh(typename List::iterator aIt, unsigned aCost)
        {
            unsigned sNewBucket = boundInc(aIt->bucket, aCost);
            auto&    sNewList   = m_Ring[sNewBucket];
            auto&    sOldList   = m_Ring[aIt->bucket];
            sNewList.splice(sNewList.end(), sOldList, aIt);
            aIt->bucket = sNewBucket;
        }

        typename List::iterator Insert(Entry&& aEntry, unsigned aCost)
        {
            unsigned sBucket = boundInc(m_Clock, aCost);
            aEntry.bucket    = sBucket;
            auto& sList      = m_Ring[sBucket];
            return sList.insert(sList.end(), std::move(aEntry));
        }

#ifdef BOOST_TEST_MESSAGE
        template <class T>
        void Debug(T&& t) const
        {
            for (auto& x : m_Ring)
                for (auto& y : x)
                    t(y);
        }
#endif
    };

    // O(1) LFU
    template <class Key, class Value>
    class LFU : public boost::noncopyable
    {
    protected:
        static constexpr unsigned BUCKET_COUNT = 1024;

        using Age = RingAge<Key, Value, BUCKET_COUNT>;
        Age m_Age;

        std::unordered_map<Key, typename Age::List::iterator> m_Keys;

        const size_t   m_MaxSize;
        const unsigned m_Cost;

        void Shrink()
        {
            if (m_Keys.size() <= m_MaxSize)
                return;
            m_Age.Shrink([this](auto& aItem) { m_Keys.erase(aItem.key); });
        }

    public:
        explicit LFU(size_t aSize, unsigned aCost = 20)
        : m_Keys(aSize)
        , m_MaxSize(aSize)
        , m_Cost(aCost)
        {}

        const Value* Get(const Key& aKey)
        {
            auto sIt = m_Keys.find(aKey);
            if (sIt == m_Keys.end())
                return nullptr;
            m_Age.Refresh(sIt->second, m_Cost);
            return &sIt->second->value;
        }

        void Put(const Key& aKey, const Value& aValue, unsigned aCost = 1)
        {
            auto sIt = m_Keys.find(aKey);
            if (sIt != m_Keys.end()) {
                sIt->second->value = aValue;
                m_Age.Refresh(sIt->second, m_Cost);
            } else {
                m_Keys[aKey] = m_Age.Insert({aKey, aValue}, aCost);
                Shrink();
            }
        }

        size_t Size() const
        {
            return m_Keys.size();
        }

#ifdef BOOST_TEST_MESSAGE
        template <class T>
        void Debug(T&& t) const
        {
            m_Age.Debug(std::forward<T>(t));
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
