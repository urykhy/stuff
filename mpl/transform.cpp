#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <typeinfo>

template<class T> using Vec = std::set<T>;

template<template <typename> class T, typename ... Ts>
struct transform;
template<template <typename> class T, typename ... Ts>
struct transform<T, std::tuple<Ts...>>
{
	using Result = std::tuple<T<Ts>...>;
};

int main(void)
{
    using T = std::tuple<int, std::string, float>;
    using N = typename transform<Vec, T>::Result;

    std::cout << typeid(T).name() << std::endl;
    std::cout << typeid(N).name() << std::endl;

    return 0;
}
