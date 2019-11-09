// https://godbolt.org/z/SP8POJ
#include <tuple>
#include <functional>

template <typename F, typename ... Ts>
bool or_elements(const F& f, const std::tuple<Ts...>& t)
{
    return std::apply([&](auto& ... ts){ return (f(ts) || ...);}, t);
}

template <typename ... Ts>
struct any_of : private std::tuple<Ts...>
{
    using std::tuple<Ts...>::tuple;
    template <typename T>
    bool operator==(const T& t) const {
        return or_elements([&t](const auto& v){ return v == t;}, *this);
    }
    template <typename T>
    bool operator<(const T& t) const {
        return or_elements([&t](const auto& v){ return v < t;}, *this);
    }
};

template <typename ... Ts>
any_of(Ts...) -> any_of<Ts...>;

void update(int& a, int& b, int& c) { a++; b++; c++; }

void func(int a, int b, int c)
{
#ifdef PLAIN
    while (a < 0 || b < 0 || c < 0)
#else
    while (any_of(a,b,c) < 0)
#endif
    {
        update(a,b,c);
    }
}

int main(void)
{
    func(1,2,3);
}
