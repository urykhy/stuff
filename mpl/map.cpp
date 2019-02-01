#include <iostream>
#include <tuple>
#include <typeinfo>

template<class T>
void my_function(const T& t)
{
    std::cout << typeid(T).name() << std::endl;
}

int main(void)
{
    using T = std::tuple<int, std::string, float>;
    T t;

    std::apply([](auto&& ...x){ (static_cast<void>(my_function(std::forward<decltype(x)>(x))), ...);}, t);

    return 0;
}
