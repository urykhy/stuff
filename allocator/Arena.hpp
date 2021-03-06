#pragma once

#include <stdexcept>
#include <string.h>
#include <boost/core/noncopyable.hpp>

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

    // simple buffer to allocate from, and release at once
    template<unsigned S>
    struct ArenaBase : public boost::noncopyable
    {
        enum
        {
            SIZE = S
        };
    private:
        typedef unsigned char BufferT[SIZE];
        size_t current = 0;
        BufferT buffer;

        template <class T>
        static T align(T aPtr, size_t aSize)
        {
            intptr_t sPtr = reinterpret_cast<intptr_t>(aPtr);
            return reinterpret_cast<T>((sPtr + aSize - 1) & ~(aSize - 1));
        }

    public:
        explicit ArenaBase()
        {
            memset(buffer, 0, SIZE);
            VALGRIND_CREATE_MEMPOOL(buffer, 0, 0);
        }

        ~ArenaBase() throw()
        {
            VALGRIND_DESTROY_MEMPOOL(buffer);
        }

        void*
        allocate(size_t n)
        {
            unsigned char* start = buffer;
            unsigned char* data = start;
            data += current;
            data = align(data, sizeof(int));

            if (data + n <= start + SIZE) {
                current = data + n - start;
                VALGRIND_MEMPOOL_ALLOC(start, data, n);
                return data;
            } else {
                throw std::bad_alloc();
            }
        }

        void
        deallocate(void* p __attribute__((unused)), size_t n __attribute__((unused)))
        {
            VALGRIND_MEMPOOL_FREE(buffer, p);
        }

        size_t used() const { return current; }

        size_t max_size() const
        {
            return SIZE - current;
        }

        void clear()
        {
            current = 0;
            VALGRIND_MEMPOOL_TRIM(buffer, 0, 0);
        }
    };

    using Arena = ArenaBase<4096>;

} // namespace Allocator
