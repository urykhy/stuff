#include <benchmark/benchmark.h>

#include <Protobuf.hpp>
#include "tutorial.pb.h"

struct MyPerson
{
    std::string name;
    std::string email;
    int32_t     id = 0;
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

static void BM_Custom(benchmark::State& state) {
    for (auto _ : state)
    {
        const Protobuf::Buffer sInput(gBuf);
        MyPerson sMyPerson;
        Protobuf::Walker sWalker(sInput);
        sWalker.parse([&sMyPerson](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker){
            switch (aField.id)
            {
            case 1: sMyPerson.name  = aWalker->readString();  break;
            case 2: sMyPerson.id    = aWalker->readVarUInt(); break;
            case 3: sMyPerson.email = aWalker->readString();  break;
            }
        });
    }
}
BENCHMARK(BM_Custom);

BENCHMARK_MAIN();
