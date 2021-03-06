#pragma once

#include <list>
#include <vector>

namespace Container
{
    template<class T>
    struct ListArray
    {
        using Array = std::vector<T>;
        using List = std::list<Array>;
    private:

        List m_List;
        const size_t m_Size;

    public:
        ListArray(const size_t c = 20 * 1024) : m_Size(c) {}

        void push_back(const T& t)
		{
            if (m_List.empty() || m_List.back().size() == m_Size)
            {
                m_List.emplace_back(Array());
                m_List.back().reserve(m_Size);
            }
            m_List.back().push_back(t);
        }

        T& back() { return m_List.back().back(); }

        void clear() { m_List.clear(); }
        size_t size() const { return m_List.empty() ? 0 : m_Size * (m_List.size() - 1) + m_List.back().size(); }

        template<class F>
        void for_chunks(F f) const { for(auto& x : m_List) f(x); }

        class const_iterator
        {
            friend struct ListArray;
            using a_iterator = typename ListArray::Array::const_iterator;
            using l_iterator = typename ListArray::List::const_iterator;
            l_iterator b;
            const l_iterator e;
            a_iterator current;

            const_iterator(l_iterator b_, l_iterator e_) : b(b_), e(e_) {
                if (b != e)
                    current = b->begin();
            }

        public:
            const T& operator*() const { return *current; }
            const T* operator->() const { return &(operator*()); }

            bool operator==(const const_iterator& r) const
            {
                return (b == e && r.b == r.e) ||
                       (b == r.b && e == r.e && current == r.current);
            }

            bool operator!=(const const_iterator& r) const
            {
                return !(*this == r);
            }

            const_iterator& operator++()
            {
                if (++current == b->end())
                {
                    if (++b != e)
                        current = b->begin();
                }
                return *this;
            }
        };

        const_iterator begin() const { return const_iterator(m_List.begin(), m_List.end()); }
        const_iterator end()   const { return const_iterator(m_List.end(),   m_List.end()); }
    };
}
