#include <cassert>
#include <iostream>
#include <tuple>
#include <type_traits>

// based on
// https://gist.github.com/utilForever/1a058050b8af3ef46b58bcfa01d5375d
// https://playfulprogramming.blogspot.com/2016/12/serializing-structs-with-c17-structured.html

namespace aux {
    struct wildcard
    {
        template <typename T, typename = std::enable_if_t<!std::is_lvalue_reference<T>::value>>
        operator T&&() const;
        template <typename T, typename = std::enable_if_t<std::is_copy_constructible<T>::value>>
        operator T&() const;
    };
    template <size_t N = 0>
    static constexpr const wildcard _{};

    template <typename T, size_t... I>
    constexpr auto is_braces_constructible_(std::index_sequence<I...>, T*) -> decltype(T{_<I>...}, std::true_type{});
    template <size_t... I>
    constexpr auto is_braces_constructible_(std::index_sequence<I...>, ...) -> std::false_type;
    template <typename T, size_t N>
    constexpr bool is_braces_constructible = std::is_same_v<std::true_type, decltype(is_braces_constructible_(std::make_index_sequence<N>{}, static_cast<T*>(nullptr)))>;
} // namespace aux

template <class T>
auto to_tuple(T&& object) noexcept
{
    using aux::is_braces_constructible;
    using type = std::decay_t<T>;
    if constexpr (is_braces_constructible<type, 4>) {
        auto&& [p1, p2, p3, p4] = object;
        return std::make_tuple(p1, p2, p3, p4);
    } else if constexpr (is_braces_constructible<type, 3>) {
        auto&& [p1, p2, p3] = object;
        return std::make_tuple(p1, p2, p3);
    } else if constexpr (is_braces_constructible<type, 2>) {
        auto&& [p1, p2] = object;
        return std::make_tuple(p1, p2);
    } else if constexpr (is_braces_constructible<type, 1>) {
        auto&& [p1] = object;
        return std::make_tuple(p1);
    } else {
        return std::make_tuple();
    }
}
int main(void)
{
    struct s
    {
        int         p1;
        double      p2;
        std::string p3;
    };

    auto t = to_tuple(s{1, 2.0, "3"});
    static_assert(std::is_same<std::tuple<int, double, std::string>, decltype(t)>{});
    assert(1 == std::get<0>(t));
    assert(2.0 == std::get<1>(t));
    assert("3" == std::get<2>(t));

    return 0;
}
