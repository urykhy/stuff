#include <benchmark/benchmark.h>

#include <google/protobuf/util/json_util.h>

// clang-format off
#include "Protobuf.hpp"
#include "Reflection.hpp"
// clang-format on

#ifdef WITH_REFLECTION
#include "ExprTK.hpp"
#endif

#include "tutorial.hpp"
#include "tutorial.pb.h"

#include <container/ListArray.hpp>

constexpr unsigned P_COUNT = 1024 * 1024;

const std::string gBuf = []() {
    tutorial::Person  sMsg;
    const std::string sJson = R"(
{
  "name": "Kevin",
  "id": 16909060,
  "email": "foo@bar.com",
  "phones": [
    {
      "number": "+1234567890",
      "type": "MOBILE"
    },
    {
      "number": "+0987654321",
      "type": "HOME"
    }
  ]
}
    )";
    google::protobuf::util::JsonStringToMessage(sJson, &sMsg);
    return sMsg.SerializeAsString();
}();

void BM_Google(benchmark::State& state)
{
    for (auto _ : state) {
        Container::ListArray<tutorial::Person> sData;
        for (unsigned i = 0; i < state.range(0); i++) {
            sData.push_back(tutorial::Person{});
            sData.back().ParseFromString(gBuf);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Google)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

void BM_Arena(benchmark::State& state)
{
    thread_local google::protobuf::Arena sArena;
    for (auto _ : state) {
        Container::ListArray<tutorial::Person*> sData;
        for (unsigned i = 0; i < state.range(0); i++) {
            auto sMsg = google::protobuf::Arena::CreateMessage<tutorial::Person>(&sArena);
            sData.push_back(sMsg);
            sData.back()->ParseFromString(gBuf);
        }
        sData.clear();
        sArena.Reset();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Arena)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

void BM_PMR(benchmark::State& state)
{
    thread_local std::pmr::monotonic_buffer_resource                   sPool(1024 * 1024);
    thread_local std::pmr::polymorphic_allocator<pmr_tutorial::Person> sAlloc(&sPool);
    for (auto _ : state) {
        Container::ListArray<pmr_tutorial::Person*> sData;
        for (unsigned i = 0; i < state.range(0); i++) {
            auto sPtr = sAlloc.allocate(1);
            sAlloc.construct(sPtr, &sPool);
            sData.push_back(sPtr);
            sData.back()->ParseFromString(gBuf);
        }
        sData.clear();
        sPool.release();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PMR)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

void BM_PMR_View(benchmark::State& state)
{
    thread_local std::pmr::monotonic_buffer_resource                       sPool(1024 * 1024);
    thread_local std::pmr::polymorphic_allocator<pmr_tutorial::PersonView> sAlloc(&sPool);
    for (auto _ : state) {
        Container::ListArray<pmr_tutorial::PersonView*> sData;
        for (unsigned i = 0; i < state.range(0); i++) {
            auto sPtr = sAlloc.allocate(1);
            sAlloc.construct(sPtr, &sPool);
            sData.push_back(sPtr);
            sData.back()->ParseFromString(gBuf);
        }
        sData.clear();
        sPool.release();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PMR_View)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

// reflection

#ifdef WITH_REFLECTION
static void BM_GoogleReflection(benchmark::State& state)
{
    tutorial::Person sPerson;
    sPerson.ParseFromString(gBuf);
    const auto* sRef   = sPerson.GetReflection();
    const auto  sField = sPerson.GetDescriptor()->FindFieldByName("id");
    for (auto _ : state)
        benchmark::DoNotOptimize(sRef->GetInt32(sPerson, sField));
}
BENCHMARK(BM_GoogleReflection);

static void BM_CustomReflection(benchmark::State& state)
{
    thread_local std::pmr::monotonic_buffer_resource sPool(1024 * 1024);

    pmr_tutorial::PersonView sPerson(&sPool);
    sPerson.ParseFromString(gBuf);
    const auto* sField = pmr_tutorial::PersonView::GetReflectionKey("id");

    auto sGetID = [&sPerson, &sField]() {
        uint32_t sVal = 0;
        sPerson.GetByID(sField->id, [&sVal](auto x) mutable {
            if constexpr (std::is_same_v<decltype(x), std::optional<int32_t>>) {
                sVal = *x;
            }
        });
        return sVal;
    };

    for (auto _ : state)
        benchmark::DoNotOptimize(sGetID());
}
BENCHMARK(BM_CustomReflection);

static void BM_Expr(benchmark::State& state)
{
    char                                sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::xtest                 sVal(&sPool);
    sVal.i32 = 123;
    Protobuf::ExprTK sExpr;
    sExpr.m_Table.create_variable("i32");
    sExpr.compile("i32 > 100 and i32 < 200");
    sExpr.resolveFrom(sVal);

    for (auto _ : state) {
        sExpr.assignFrom(sVal);
        benchmark::DoNotOptimize(sExpr.eval());
    }
}
BENCHMARK(BM_Expr);
#endif // WITH_REFLECTION

BENCHMARK_MAIN();