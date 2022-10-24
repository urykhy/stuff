#pragma once

#include <concepts>
#include <type_traits>

#include "cbor-internals.hpp"

namespace cbor {
    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::cbor_read)>
    void read(istream& in, T& t) { t.cbor_read(in); }

    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::cbor_write)>
    void write(ostream& out, const T& t) { t.cbor_write(out); }
} // namespace cbor