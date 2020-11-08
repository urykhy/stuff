#pragma once

#include "Arena.hpp"
#include "Face.hpp"
#include "Pool.hpp"

namespace Allocator {

    // generic
    template <class T, class A>
    struct SmallObject
    {
        using Allocator = A;
        static void* operator new(std::size_t s)
        {
            assert(s == sizeof(T));
            auto x = Guard<Allocator>::get();
            assert(x);
            return x->allocate(s);
        }
        static void operator delete(void* a, std::size_t s)
        {
            assert(s == sizeof(T));
            auto x = Guard<Allocator>::get();
            assert(x);
            x->deallocate((T*)a, s);
        }
    };

    // impl for face + arena
    template <class T>
    struct SmallObject<T, Face<char, Arena>> : SmallObject<T, Arena>
    {};

} // namespace Allocator
