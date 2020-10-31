#pragma once
#include <memory>

#include "cbor-tuple.hpp"

namespace cbor {
    namespace aux {

        template <typename S>
        struct signature : public signature<decltype(&S::operator())>
        {};
        template <typename R, typename... Args>
        struct signature<R (*)(Args...)>
        {
            using return_type   = R;
            using argument_type = std::tuple<Args...>;
        };
        template <typename C, typename R, typename... Args>
        struct signature<R (C::*)(Args...) const>
        {
            using return_type   = R;
            using argument_type = std::tuple<Args...>;
        };

        template <typename F, typename Tuple, size_t... I>
        decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>)
        {
            return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
        }
        template <typename F, typename Tuple>
        decltype(auto) apply(F&& f, Tuple&& t)
        {
            using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
            return apply_impl(std::forward<F>(f), std::forward<Tuple>(t), Indices{});
        }

        struct proxy_i
        {
            virtual void run(cbor::istream& in, cbor::ostream& out) = 0;
            virtual ~proxy_i() throw(){};
        };

        template <class F>
        class proxy : public proxy_i
        {
            F func;

        public:
            proxy(F f)
            : func(f)
            {}

            virtual void run(cbor::istream& in, cbor::ostream& out)
            {
                typename signature<decltype(func)>::argument_type arg;
                cbor::read(in, arg);
                auto x = cbor::aux::apply(func, arg);
                cbor::write(out, x);
            }
        };
    } // namespace aux
    typedef std::shared_ptr<aux::proxy_i> proxy_ptr;

    template <class F>
    auto make_proxy(F f)
    {
        return std::make_shared<aux::proxy<F>>(f);
    }
} // namespace cbor