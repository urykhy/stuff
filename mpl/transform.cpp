#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <typeinfo>

#include <boost/type_index.hpp>

template <template <class...> class T, typename... Ts>
struct transform;
template <template <class...> class T, typename... Ts>
struct transform<T, std::tuple<Ts...>>
{
    using Result = std::tuple<T<Ts>...>;
};

int main(void)
{
    using T = std::tuple<int, std::string, float>;
    using N = typename transform<std::set, T>::Result;

    std::cout << boost::typeindex::type_id<T>() << std::endl;
    std::cout << boost::typeindex::type_id<N>() << std::endl;

    return 0;
}
