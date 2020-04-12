#pragma once

#include <tuple>

namespace Mpl
{
    //  call f with every argument from pack
    template <class F, class... T>
    void for_each_argument(F f, T&&... a)
    {
        (void)std::initializer_list<int>{ (f(std::forward<T>(a)), 0)... };
    }

    //  call f with every argument from tuple
    //  using T = std::tuple<int, float, std::string>
    //  for_each_tuple_element([](auto&& x){ std::cout << typeid(x).name() << std::endl; }, T{});
    template<typename F, typename Tuple>
    void for_each_element(F&& f, Tuple&& aTuple)
    {
        std::apply([f = std::move(f)] (auto... x) {(f(x),...);}, aTuple);
    }
}
