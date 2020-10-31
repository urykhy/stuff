#pragma once
#include <tuple>

#include <mpl/Mpl.hpp>

#include "decoder.hpp"
#include "encoder.hpp"

namespace cbor {
    template <typename>
    struct is_tuple : std::false_type
    {};

    template <typename... T>
    struct is_tuple<std::tuple<T...>> : std::true_type
    {};

    template <class T>
    typename std::enable_if<is_tuple<T>::value, void>::type
    write(ostream& out, const T&& t)
    {
        write_type_value(out, CBOR_LIST, std::tuple_size<T>::value);
        Mpl::for_each_element(
            [&out](const auto& x) {
                write(out, x);
            },
            t);
    }

    template <class T>
    typename std::enable_if<is_tuple<T>::value, void>::type
    read(istream& in, T& t)
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

    template <class... T>
    void write(ostream& out, const T&&... t)
    {
        write_type_value(out, CBOR_LIST, sizeof...(t));
        Mpl::for_each_argument(
            [&out](const auto& x) {
                write(out, x);
            },
            t...);
    }

    template <class... T>
    void read(istream& in, T&... t)
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
} // namespace cbor
