#pragma once

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

    namespace aux {
        template <class T, size_t SIZE>
        class Buf
        {
            T buffer[SIZE];
            size_t pos;

        public:
            Buf()
            : pos(0)
            {}

            T* allocate()
            {
                if (pos < SIZE) {
                    return &buffer[pos++];
                }
                return 0;
            };

            bool empty() const { return pos >= SIZE; }
        };
    } // namespace aux

    // pool of small objects
    template <class T, size_t BUFSIZE = 128>
    class Pool
    {
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;

        union Entry
        {
            char pad[sizeof(T)];
            Entry* next;
        };

        typedef aux::Buf<Entry, BUFSIZE> PoolEntry;
        std::list<PoolEntry*> pool;
        Entry* head;

    public:
        Pool()
        : head(0)
        {
            ;
        }
        ~Pool() throw()
        {
            for (auto& x : pool) {
                delete x;
            }
        }
        // abs minimal: rebind, allocate, deallocate
        template <typename _Tp1>
        struct rebind
        {
            typedef Pool<_Tp1> other;
        };

        T* allocate(size_t s)
        {
            Entry* n = 0;
            if (!head) {
                if (pool.empty() || pool.back()->empty())
                    pool.push_back(new PoolEntry());
                n = pool.back()->allocate();
                VALGRIND_MALLOCLIKE_BLOCK(n, sizeof(Entry), 0, 1);
                memset(n, 0, sizeof(Entry));
            } else {
                n = head;
                VALGRIND_MALLOCLIKE_BLOCK(n, sizeof(Entry), 0, 1);
                head = head->next;
                memset(n, 0, sizeof(Entry));
            }
            return reinterpret_cast<T*>(n);
        }
        void deallocate(T* t, size_t = 0)
        {
            Entry* e = reinterpret_cast<Entry*>(t);
            e->next = head;
            head = e;
            VALGRIND_FREELIKE_BLOCK(head, 0);
        }

        size_t size() const { return pool.size(); }
        size_t mem_used() const { return size() * sizeof(Entry) * BUFSIZE; };

        // it can fire valgrind if build with CONFIG_WITH_VALGRIND
        size_t free_blocks() const
        {
            size_t n = 0;
            for (Entry* e = head; e; e = e->next) {
                n++;
            }
            return n;
        }
    };

} // namespace Allocator
