#pragma once

namespace Allocator {

    // some helpers to avoid passing allocator in arguments
    template <class T>
    class Guard
    {
        Guard(const Guard& a);
        Guard& operator=(const Guard& a);

        static __thread T* current;
        T* old;

    public:
        static T* get()
        {
            return current;
        }

        Guard(T* a)
        {
            old = current;
            current = a;
        }
        ~Guard() throw()
        {
            current = old;
        }
    };
    template <class T>
    __thread T* Guard<T>::current = 0;

    template <class T, class I>
    class Face
    {
    public:
        typedef size_t size_type;
        typedef std::ptrdiff_t difference_type;
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef T value_type;
        typedef I Impl;

        template <typename _Tp1>
        struct rebind
        {
            typedef Face<_Tp1, Impl> other;
        };

        Face()
        {
        }

        Face(Impl* buf_)
        {
        }

        Face(const Face& /*a*/)
        {
        }

        template <class R>
        Face(const Face<R, Impl>& /*a*/)
        {
        }

        ~Face() throw()
        {
        }

        pointer
        allocate(size_type n, const void* = 0)
        {
            Impl* buf = Guard<Impl>::get();
            assert(buf);
            return static_cast<pointer>(buf->allocate(n * sizeof(T)));
        }

        void
        deallocate(pointer p, size_type n)
        {
            Impl* buf = Guard<Impl>::get();
            assert(buf);
            buf->deallocate(p, n);
        }

        pointer
        address(reference x) const
        {
            return &x;
        }

        const_pointer
        address(const_reference x) const
        {
            return &x;
        }

        void
        construct(pointer p, const T& __val)
        {
            new (p) T(__val);
        }

        void
        destroy(pointer p)
        {
            p->~T();
        }

        size_t max_size() const
        {
            Impl* buf = Guard<Impl>::get();
            assert(buf);
            if (!buf)
                return 0;
            return buf->max_size() / sizeof(T);
        }
    };

    template <typename _Tp1, typename _Tp2, typename _I>
    inline bool
    operator==(const Face<_Tp1, _I>& /*a*/, const Face<_Tp2, _I>& /*b*/)
    {
        return true;
    }

    template <typename _Tp1, typename _Tp2, typename _I1, typename _I2>
    inline bool
    operator==(const Face<_Tp1, _I1>& /*a*/, const Face<_Tp2, _I2>& /*b*/)
    {
        return false;
    }

    template <typename _Tp1, typename _Tp2, typename _I1, typename _I2>
    inline bool
    operator!=(const Face<_Tp1, _I1>& a, const Face<_Tp2, _I2>& b)
    {
        return !(a == b);
    }
} // namespace Allocator
