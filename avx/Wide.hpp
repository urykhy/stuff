
#pragma once

#include "immintrin.h"

//#define STR_HELPER(x) #x
//#define STR(x) STR_HELPER(x)

#define MAKE_AVX_INT_VECTOR(n)                                              \
class WideInt##n {                                                          \
    __m256i data;                                                           \
public:                                                                     \
    WideInt##n(uint##n##_t t = 0) {                                         \
        data = _mm256_set1_epi##n(t);                                       \
    }                                                                       \
    WideInt##n& operator=(const WideInt##n & w) {                           \
        data = w.data;                                  return *this;       \
    }                                                                       \
    WideInt##n& operator+=(const WideInt##n & other) {                      \
        data = _mm256_add_epi##n(data, other.data);       return *this;     \
    }                                                                       \
    WideInt##n& operator-=(const WideInt##n & other) {                      \
        data = _mm256_sub_epi##n(data, other.data);       return *this;     \
    }                                                                       \
    WideInt##n& operator==(const WideInt##n & other) {                      \
        data = _mm256_cmpeq_epi##n(data, other.data);     return *this;     \
    }                                                                       \
    WideInt##n& operator>=(const WideInt##n & other) {                      \
        data = _mm256_cmpgt_epi##n(data, other.data);     return *this;     \
    }                                                                       \
    WideInt##n& operator&=(const WideInt##n & other) {                      \
        data = _mm256_and_si256(data, other.data);        return *this;     \
    }                                                                       \
    WideInt##n& operator|=(const WideInt##n & other) {                      \
        data = _mm256_or_si256(data, other.data);         return *this;     \
    }                                                                       \
    WideInt##n& operator^=(const WideInt##n & other) {                      \
        data = _mm256_xor_si256(data, other.data);        return *this;     \
    }                                                                       \
    uint64_t sum() const {                                                  \
        uint64_t res = 0;                                                   \
        uint##n##_t* ptr = (uint##n##_t*)&data;                             \
        for (auto i = 0; i < 256 / n; i++)                                  \
            res += ptr[i];                                                  \
        return res;                                                         \
    }                                                                       \
}

MAKE_AVX_INT_VECTOR(8);
MAKE_AVX_INT_VECTOR(16);
MAKE_AVX_INT_VECTOR(32);
#define _mm256_set1_epi64 _mm256_set1_epi64x
MAKE_AVX_INT_VECTOR(64);
#undef MAKE_AVX_INT_VECTOR

#define MAKE_AVX_FLOAT_VECTOR(n, su, tname, avxt)                           \
class WideFloat##n {                                                        \
    avxt data;                                                              \
public:                                                                     \
    WideFloat##n(tname t = 0) {                                             \
        data = _mm256_set1_##su(t);                                         \
    }                                                                       \
    WideFloat##n& operator=(const WideFloat##n & w) {                       \
        data = w.data;                                  return *this;       \
    }                                                                       \
    WideFloat##n& operator+=(const WideFloat##n & other) {                  \
        data = _mm256_add_##su(data, other.data);       return *this;       \
    }                                                                       \
    WideFloat##n& operator+=(const avxt& other) {                           \
        data = _mm256_add_##su(data, other);            return *this;       \
    }                                                                       \
    WideFloat##n& operator-=(const WideFloat##n & other) {                  \
        data = _mm256_sub_##su(data, other.data);       return *this;       \
    }                                                                       \
    WideFloat##n& operator==(const WideFloat##n & other) {                  \
        data = _mm256_cmp_##su(data, other.data, _CMP_EQ_OQ); return *this; \
    }                                                                       \
    WideFloat##n& operator>=(const WideFloat##n & other) {                  \
        data = _mm256_cmp_##su(data, other.data, _CMP_GE_OQ); return *this; \
    }                                                                       \
    WideFloat##n& operator<=(const WideFloat##n & other) {                  \
        data = _mm256_cmp_##su(data, other.data, _CMP_LE_OQ); return *this; \
    }                                                                       \
    WideFloat##n& max(const WideFloat##n & other) {                         \
        data = _mm256_max_##su(data, other.data);       return *this;       \
    }                                                                       \
    WideFloat##n& min(const WideFloat##n & other) {                         \
        data = _mm256_min_##su(data, other.data);       return *this;       \
    }                                                                       \
    double sum() const {                                                    \
        double res = 0;                                                     \
        tname* ptr = (tname*)&data;                                         \
        for (auto i = 0; i < 256 / n; i++)                                  \
            res += ptr[i];                                                  \
        return res;                                                         \
    }                                                                       \
}

MAKE_AVX_FLOAT_VECTOR(32, ps, float,  __m256);
MAKE_AVX_FLOAT_VECTOR(64, pd, double, __m256d);
#undef MAKE_AVX_FLOAT_VECTOR

