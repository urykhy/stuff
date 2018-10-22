#include <benchmark/benchmark.h>

#include <Protobuf.hpp>
#include "tutorial.pb.h"

struct MyPerson
{
    std::string name;
    std::string email;
    int32_t     id = 0;

    void Clear()
    {
        name.clear();
        email.clear();
        id = 0;
    }

    void ParseFromString(const std::string& aString)
    {
        Clear();
        const Protobuf::Buffer sInput(aString);
        Protobuf::Walker sWalker(sInput);
        sWalker.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker) -> Protobuf::Action {
            switch (aField.id)
            {
                case 1: aWalker->read(name);  return Protobuf::ACT_USED;
                case 2: aWalker->read(id);    return Protobuf::ACT_USED;
                case 3: aWalker->read(email); return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
        });
    }
};

std::string gBuf("\x0a\x05\x4b\x65\x76\x69\x6e\x10\x84\x86\x88\x08\x1a\x0b\x66\x6f\x6f\x40\x62\x61\x72\x2e\x63\x6f\x6d");

static void BM_Google(benchmark::State& state) {
    for (auto _ : state)
    {
        tutorial::Person sPerson;
        sPerson.ParseFromString(gBuf);
    }
}
BENCHMARK(BM_Google);

static void BM_Google_reuse(benchmark::State& state) {
    tutorial::Person sPerson;
    for (auto _ : state)
        sPerson.ParseFromString(gBuf);
}
BENCHMARK(BM_Google_reuse);

static void BM_Custom(benchmark::State& state) {
    MyPerson sMyPerson;
    for (auto _ : state)
        sMyPerson.ParseFromString(gBuf);
}
BENCHMARK(BM_Custom);

BENCHMARK_MAIN();
