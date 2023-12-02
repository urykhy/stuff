#pragma once

#include <tuple>

namespace Mpl {
    //  call f with every argument from pack
    template <class F, class... T>
    void for_each_argument([[maybe_unused]] F f, T&&... a)
    {
        (void)std::initializer_list<int>{(f(std::forward<T>(a)), 0)...};
    }

    //  call f with every argument from tuple
    //  using T = std::tuple<int, float, std::string>
    //  for_each_tuple_element([](auto&& x){ std::cout << typeid(x).name() << std::endl; }, T{});
    template <typename F, typename Tuple>
    void for_each_element(F&& f, Tuple&& aTuple)
    {
        std::apply([f = std::move(f)](auto&... x) mutable { (f(x), ...); }, aTuple);
    }

    // combine few functors to one
    template <class... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    // signature from function (used in cbor-proxy)
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

} // namespace Mpl
