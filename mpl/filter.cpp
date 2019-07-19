#include <iostream>
#include <string>
#include <tuple>


template <typename, typename> struct Cons;
template <typename  T, typename ...Args>
struct Cons<T, std::tuple<Args...>>
{
    using type = std::tuple<T, Args...>;
};

template <typename...> struct filter;
template <> struct filter<> { using type = std::tuple<>; };

template <typename Head, typename ...Tail>
struct filter<Head, Tail...>
{
    using type = typename std::conditional<std::is_same_v<Head, int>,
          typename Cons<Head, typename filter<Tail...>::type>::type,
          typename filter<Tail...>::type
          >::type;
};

template<typename... Args>
struct filter<std::tuple<Args...>>
{
    using type = typename filter<Args...>::type;
};

int main() {

    using T = std::tuple<int, std::string, float>;
    using R = filter<T>::type;
    //using R = filter<int, float, std::string, int>::type;

    std::cout << std::tuple_size<R>() << std::endl;
    std::cout << typeid(R).name() << std::endl;
}
