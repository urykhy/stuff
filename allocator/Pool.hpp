#pragma once

#include <vector>

#ifdef CONFIG_WITH_VALGRIND
#include <valgrind/valgrind.h>
#else
#ifndef VALGRIND_CREATE_MEMPOOL
#define VALGRIND_CREATE_MEMPOOL(a, b, c)
#define VALGRIND_DESTROY_MEMPOOL(a)
#define VALGRIND_MEMPOOL_ALLOC(a, b, c)
#define VALGRIND_MEMPOOL_FREE(a, b)
#define VALGRIND_MEMPOOL_TRIM(a, b, c)
#define VALGRIND_MALLOCLIKE_BLOCK(a, b, c, d)
#define VALGRIND_FREELIKE_BLOCK(a, b)
#endif
#endif

namespace Allocator {

    // fixed pool of small objects
    template <class T>
    class Pool
    {
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;

        union Entry
        {
            char   pad[sizeof(T)];
            Entry* next;
        };
        std::vector<char> m_Data;
        Entry*            m_Head  = nullptr;
        uint32_t          m_Avail = 0;

    public:
        Pool(size_t aSize = 64 * 1024 /* buffer size in bytes */)
        : m_Data(aSize, 0)
        {
            m_Avail = aSize / sizeof(Entry);
            for (unsigned i = 0; i < m_Avail; i++) {
                Entry* x = (Entry*)&m_Data[sizeof(Entry) * i];
                x->next  = m_Head;
                m_Head   = x;
            }
        }
        ~Pool() throw()
        {}

        T* allocate(size_t s)
        {
            if (m_Head == nullptr)
                throw std::bad_alloc();
            assert(m_Avail > 0);

            Entry* sResult = m_Head;
            VALGRIND_MALLOCLIKE_BLOCK(sResult, sizeof(Entry), 0, 1);
            m_Head = m_Head->next;
            memset(sResult, 0, sizeof(Entry));
            m_Avail--;

            return reinterpret_cast<T*>(sResult);
        }
        void deallocate(T* aAddr, size_t = 0)
        {
            Entry* sEntry = reinterpret_cast<Entry*>(aAddr);
            sEntry->next  = m_Head;
            m_Head        = sEntry;
            m_Avail++;
            VALGRIND_FREELIKE_BLOCK(m_Head, 0);
        }

        size_t size() const { return m_Data.size() / sizeof(Entry); }
        size_t avail() const { return m_Avail; }
    };

} // namespace Allocator
