#pragma once
#include <tuple>

#include "cbor-basic.hpp"

#include <mpl/Mpl.hpp>

namespace cbor {
    template <typename>
    struct is_tuple : std::false_type
    {};

    template <typename... T>
    struct is_tuple<std::tuple<T...>> : std::true_type
    {};

    template <class S, class T>
    typename std::enable_if<is_tuple<T>::value, void>::type
    read(S& in, T& t)
    {
        const auto sCount = get_uint(in, ensure_type(in, CBOR_LIST));
        if (sCount != std::tuple_size<T>::value) {
            throw std::runtime_error("bad number of elements");
        }
        Mpl::for_each_element(
            [&in](auto& x) {
                read(in, x);
            },
            t);
    }

    template <class T>
    typename std::enable_if<is_tuple<T>::value, void>::type
    write(ostream& out, const T& t)
    {
        write_type_value(out, CBOR_LIST, std::tuple_size<T>::value);
        Mpl::for_each_element(
            [&out](const auto& x) {
                write(out, x);
            },
            t);
    }

    template <class S, class... T>
    typename std::enable_if<(sizeof...(T) > 1), void>::type
    read(S& in, T&... t)
    {
        const auto sCount = get_uint(in, ensure_type(in, CBOR_LIST));
        if (sCount != sizeof...(t)) {
            throw std::runtime_error("bad number of elements");
        }
        Mpl::for_each_argument(
            [&in](auto& x) {
                read(in, x);
            },
            t...);
    }

    template <class... T>
    typename std::enable_if<(sizeof...(T) > 1), void>::type
    write(ostream& out, const T&... t)
    {
        write_type_value(out, CBOR_LIST, sizeof...(t));
        Mpl::for_each_argument(
            [&out](const auto& x) {
                write(out, x);
            },
            t...);
    }
} // namespace cbor
