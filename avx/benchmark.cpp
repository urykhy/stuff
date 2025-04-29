#include <benchmark/benchmark.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <random>

#include <boost/align/aligned_allocator.hpp>

#include "Search.hpp"
#include "Wide.hpp"

// sum numbers

const std::vector<float, boost::alignment::aligned_allocator<float, 256>> sSumData = []() {
    std::vector<float, boost::alignment::aligned_allocator<float, 256>> v;
    v.resize(1024 * 1024);
    return v;
}();

static void BM_SUM_SCALAR(benchmark::State& state)
{
    for (auto _ : state) {
        float res = 0;
        for (const auto& x : sSumData) {
            benchmark::DoNotOptimize(res += x);
        }
    }
}
BENCHMARK(BM_SUM_SCALAR);

static void BM_SUM_AVX(benchmark::State& state)
{
    for (auto _ : state) {
        WideFloat32   res1;
        const __m256* beg = (__m256*)(&sSumData.front());
        const __m256* end = (__m256*)(&sSumData.back() + 1);

        while (beg < end) {
            res1 += *beg;
            beg++;
        }
        benchmark::DoNotOptimize(res1.sum());
    }
}
BENCHMARK(BM_SUM_AVX);

// binary search / lower bound
// 1 block = 8 32bit ints (256 bit)

constexpr unsigned INTS_PER_BLOCK = 8; // 8*32 = 256 = avx register size

const auto sMake = [](unsigned aCount) {
    Util::alignedVector<uint32_t> sData;
    const unsigned                COUNT = aCount * INTS_PER_BLOCK;
    sData.reserve(COUNT);
    for (unsigned i = 0; i < COUNT; i++)
        sData.push_back(i);
    return sData;
};

const auto sShuffle = [](const Util::alignedVector<uint32_t>& aData) {
    std::random_device sDevice;
    std::mt19937       sGen(sDevice());

    Util::alignedVector<uint32_t> sData = aData;
    std::shuffle(sData.begin(), sData.end(), sGen);
    return sData;
};

static void BM_SEARCH_STD(benchmark::State& state)
{
    srand48(123);
    const unsigned BLOCK_COUNT = state.range(0);
    const auto     sVec        = sMake(BLOCK_COUNT);
    const auto     sRand       = sShuffle(sVec);
    unsigned       i           = 0;
    for (auto _ : state) {
        i++;
        if (i == 8 * BLOCK_COUNT)
            i = 0;
        benchmark::DoNotOptimize(std::lower_bound(sVec.begin(), sVec.end(), sRand[i]));
    }
}
BENCHMARK(BM_SEARCH_STD)->Arg(1)->Arg(4)->Arg(64)->Arg(4096)->Arg(262144)->Arg(2097152);

static void BM_SEARCH_AVX(benchmark::State& state)
{
    srand48(123);
    const unsigned BLOCK_COUNT = state.range(0);
    const auto     sVec        = sMake(BLOCK_COUNT);
    const auto     sRand       = sShuffle(sVec);

    state.counters["order"]         = std::log2(BLOCK_COUNT * INTS_PER_BLOCK);
    state.counters["workset_bytes"] = BLOCK_COUNT * INTS_PER_BLOCK * sizeof(uint32_t);

    std::unique_ptr<Util::avxIndex> sAvxIndex;
    if (BLOCK_COUNT > 4) {
        sAvxIndex                      = std::make_unique<Util::avxIndex>(sVec);
        state.counters["index_bytes"]  = sAvxIndex->memory();
        state.counters["index_height"] = sAvxIndex->height();
    }

    auto sLowerBound = [&]() -> std::function<int(uint32_t aVal)> {
        const WideInt32* sInput = reinterpret_cast<const WideInt32*>(&sVec[0]);
        if (BLOCK_COUNT == 1)
            return [sInput](uint32_t aVal) {
                return Util::avxLowerBound1(*sInput, aVal);
            };
        else if (BLOCK_COUNT == 4)
            return [sInput](uint32_t aVal) {
                return Util::avxLowerBound4(*sInput, aVal);
            };
        else // BLOCK_COUNT > 4
            return [&sAvxIndex](uint32_t aVal) {
                return sAvxIndex->lower_bound(aVal);
            };
    }();

    unsigned i = 0;
    for (auto _ : state) {
        i++;
        if (i == 8 * BLOCK_COUNT)
            i = 0;
        benchmark::DoNotOptimize(sLowerBound(sRand[i]));
    }
}
BENCHMARK(BM_SEARCH_AVX)->Arg(1)->Arg(4)->Arg(64)->Arg(4096)->Arg(262144)->Arg(2097152);

BENCHMARK_MAIN();
