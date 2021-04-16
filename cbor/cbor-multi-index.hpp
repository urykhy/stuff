#pragma once

#include <boost/multi_index_container_fwd.hpp>

#include "cbor-basic.hpp"

namespace cbor {

    template <class T, class I, class A>
    void read(istream& s, boost::multi_index_container<T, I, A>& t)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_LIST));
        t.clear();
        for (size_t i = 0; i < mt; i++) {
            T tmp;
            read(s, tmp);
            t.insert(std::move(tmp));
        }
    }

    template <class T, class I, class A>
    void write(ostream& out, const boost::multi_index_container<T, I, A>& t)
    {
        write_type_value(out, CBOR_LIST, t.size());
        for (auto& x : t)
            write(out, x);
    }

} // namespace cbor
