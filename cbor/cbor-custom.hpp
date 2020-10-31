#pragma once

#include <type_traits>

#include "cbor-internals.hpp"

namespace cbor {

    // helpers to call t.cbor_{read,write} if available
    template <typename T, typename = void>
    struct has_read : std::false_type
    {};

    template <typename T>
    struct has_read<T, std::void_t<decltype(&T::cbor_read)>> : std::true_type
    {};

    template <class T>
    constexpr bool has_read_v = has_read<T>::value;

    template <class T>
    typename std::enable_if<has_read_v<T>, void>::type read(istream& in, T& t) { t.cbor_read(in); }

    template <typename T, typename = void>
    struct has_write : std::false_type
    {};

    template <typename T>
    struct has_write<T, std::void_t<decltype(&T::cbor_write)>> : std::true_type
    {};

    template <class T>
    constexpr bool has_write_v = has_write<T>::value;

    template <class T>
    typename std::enable_if<has_write_v<T>, void>::type write(ostream& out, const T& t) { t.cbor_write(out); }

} // namespace cbor