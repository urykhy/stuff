#include <iostream>
#include <tuple>
#include <typeinfo>

template<typename Tuple, typename F>
void foreach(F&& f)
{
    std::apply([f=std::move(f)](auto ...x)
    {
        (f(x),...);
    }, Tuple{});
}
int main(void)
{
    using T = std::tuple<int, float, std::string>;
    foreach<T>([](auto&& x){
        std::cout << typeid(x).name() << std::endl;
    });
    return 0;
}
