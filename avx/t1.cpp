
// g++ -std=c++14 -O3 t1.cpp -mavx2 -I../../include -I. -I../aio

#include <iostream>
#include <vector>
#include <TimeMeter.hpp>

#include <string.h>
#include "Wide.hpp"

double crap = 0;

void f1(const std::vector<float>& v)
{
    double res = 0;

    Util::TimeMeter t;
    for (const auto& x : v) {
        res += x;
    }
    std::cout << "scalar: " << (size_t)res << " in " << t.get().to_double() << std::endl;
    crap += res;
}

void f2(const std::vector<float>& v)
{
    const __m256* beg = (__m256*)(&v.front());
    const __m256* end = (__m256*)(&v.back() + 1);

    WideFloat32 res1;

    Util::TimeMeter t;
    while (beg < end) {
        res1 += *beg;
        beg++;
    }
    auto res = res1.sum();

    std::cout << "avx2:   " << (size_t)res << " in " << t.get().to_double() << std::endl;
    crap += res;
}


int main(void)
{
    std::vector<float> v;
    v.resize(1024 * 1024 * 102);

    for (auto& x : v) {
        x = 1;
    }

    f1(v);
    f2(v);
    (void)crap;
}

