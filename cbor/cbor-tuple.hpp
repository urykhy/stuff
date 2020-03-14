#pragma once
#include <tuple>
namespace cbor {

    namespace aux {
        template <class F, class... Ts>
        void for_each_argument(F f, Ts&&... a) {
            (void)std::initializer_list<int>{(f(std::forward<Ts>(a)), 0)...};
        }

        template<typename F, typename T, std::size_t... I>
        inline void
        tuple_for_each_int(F f, T& t, std::index_sequence<I...>)
        {
            (void)std::initializer_list<int>{
                (f(std::get<I>(t)), 0)...
            };
        }
        template<typename F, typename T, typename I = std::make_index_sequence<std::tuple_size<T>::value>>
        inline void
        tuple_for_each(F f, T& t)
        {
            tuple_for_each_int(f, t, I());
        }
    }

    //

    template<class... T>
    void write(omemstream& out, const T&&... t) {
        cbor::write_type_value(out, CBOR_LIST, sizeof...(t));
        aux::for_each_argument([&out](const auto& x){
            write(out, x);
        }, t...);
    }

    template<class... T>
    void read(imemstream& in, T&... t) {
        if (cbor::get_uint(in, cbor::ensure_type(in, CBOR_LIST)) != sizeof...(t)) {
            throw std::runtime_error ("bad number of elements");
        }
        aux::for_each_argument([&in](auto& x){
            read(in, x);
        }, t...);
    }

    //

    template<class... T>
    void write(omemstream& out, const std::tuple<T...>& t) {
        cbor::write_type_value(out, CBOR_LIST, std::tuple_size<std::tuple<T...>>::value);
        aux::tuple_for_each([&out](const auto& x){
            write(out, x);
        }, t);
    }

    template<class... T>
    void read(imemstream& in, std::tuple<T...>& t) {
        if (cbor::get_uint(in, cbor::ensure_type(in, CBOR_LIST)) != std::tuple_size<std::tuple<T...>>::value) {
            throw std::runtime_error ("bad number of elements");
        }
        aux::tuple_for_each([&in](auto& x){
            read(in, x);
        }, t);
    }
}
