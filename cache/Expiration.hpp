#pragma once

namespace Cache {

    template <class Key, class Value, template <class, class> class Cache>
    class ExpirationAdapter
    {
        struct Entry
        {
            uint64_t created_at{0}; // ms
            Value    value = {};
        };
        Cache<Key, Entry> m_Cache;
        const uint64_t    m_Deadline; // ms

    public:
        ExpirationAdapter(size_t aMaxSize, uint64_t aDeadline)
        : m_Cache(aMaxSize)
        , m_Deadline(aDeadline)
        {
        }

        const Value* Get(const Key& aKey, const uint64_t aNow)
        {
            auto sPtr = m_Cache.Get(aKey);
            if (sPtr == nullptr) {
                return nullptr;
            }
            if (sPtr->created_at + m_Deadline <= aNow) { // expired
                m_Cache.Remove(aKey);
                return nullptr;
            }
            return &sPtr->value;
        }

        void Put(const Key& aKey, const Value& aValue, const uint64_t aNow)
        {
            m_Cache.Put(aKey, Entry{.created_at = aNow,
                                    .value      = aValue});
        }

        void Remove(const Key& aKey)
        {
            m_Cache.Remove(aKey);
        }

        size_t Size() const
        {
            return m_Cache.Size();
        }
    };
} // namespace Cache
