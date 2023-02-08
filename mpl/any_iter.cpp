
#include <any>
#include <cassert>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

std::vector<std::any> prepare()
{
    using A1 = std::vector<int>;
    using A2 = std::vector<std::string>;
    using A3 = std::vector<float>;
    A1 a1;
    A2 a2;
    A3 a3;
    for (int i = 0; i < 10; i++) {
        a1.push_back(i);
        a2.push_back("x:" + std::to_string(i));
        a3.push_back(i * 3.14);
    }
    std::vector<std::any> a;
    a.push_back(std::any(a1));
    a.push_back(std::any(a2));
    a.push_back(std::any(a3));
    return a;
}

template <typename T, typename A, std::size_t... Indices>
auto fromAny(const A& a, std::index_sequence<Indices...>)
{
    return std::make_tuple(std::any_cast<std::vector<std::tuple_element_t<Indices, T>>>(a[Indices])...);
}

template <typename V>
auto tupleBegin(const V& v)
{
    return std::apply([](auto&... x) mutable { return std::make_tuple(std::begin(x)...); }, v);
}

template <typename V>
auto tupleEnd(const V& v)
{
    return std::apply([](auto&... x) mutable { return std::make_tuple(std::end(x)...); }, v);
}

template <typename Iter>
void tupleAdvance(Iter& i)
{
    std::apply([](auto&... x) mutable { (std::advance(x, 1), ...); }, i);
}

template <typename V>
auto tupleAccess(const V& v)
{
    return std::apply([](auto&... x) mutable { return std::make_tuple(std::ref(*x)...); }, v);
}

template <class T, class A, class H>
void iterate(const A& a, H&& h)
{
    assert(std::tuple_size_v<T> <= a.size());
    using I = std::make_index_sequence<std::tuple_size_v<T>>;
    auto v  = fromAny<T>(a, I());

    for (auto i = tupleBegin(v); i != tupleEnd(v); tupleAdvance(i)) {
        h(tupleAccess(i));
    }
}

int main(void)
{
    auto a = prepare();

    using Tuple = std::tuple<int, std::string, float>;

    iterate<Tuple>(a, [](auto item) {
        std::cout << '('
                  << std::get<0>(item) << ','
                  << std::get<1>(item) << ','
                  << std::get<2>(item)
                  << ')' << std::endl;
    });

    return 0;
}