#include <benchmark/benchmark.h>

#include <Protobuf.hpp>
#include "tutorial.pb.h"

#include <vector>

struct PhoneNumber
{
    std::string number;
    int         type = 1;

    void Clear()
    {
        number.clear();
        type = 1;
    }

    void ParseFromString(const Protobuf::Buffer& aString)
    {
        Protobuf::Walker sWalker(aString);
        sWalker.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker) -> Protobuf::Action {
            switch (aField.id)
            {
                case 1: aWalker->read(number);  return Protobuf::ACT_USED;
                case 2: aWalker->read(type);    return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
        });
    }
};

struct MyPerson
{
    std::string name;
    std::string email;
    int32_t     id = 0;
    std::vector<PhoneNumber> phones;

    void Clear()
    {
        name.clear();
        email.clear();
        id = 0;
        phones.clear();
    }

    void ParseFromString(const Protobuf::Buffer& aString)
    {
        Clear();
        Protobuf::Walker sWalker(aString);
        sWalker.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker) -> Protobuf::Action {
            switch (aField.id)
            {
                case 1: aWalker->read(name);  return Protobuf::ACT_USED;
                case 2: aWalker->read(id);    return Protobuf::ACT_USED;
                case 3: aWalker->read(email); return Protobuf::ACT_USED;
                case 4:
                        {
                            Protobuf::Buffer sTmpBuf;
                            aWalker->read(sTmpBuf);
                            phones.push_back(PhoneNumber{});
                            phones.back().ParseFromString(sTmpBuf);
                        }
                        return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
        });
    }
};

//const std::string gBuf("\x0a\x05\x4b\x65\x76\x69\x6e\x10\x84\x86\x88\x08\x1a\x0b\x66\x6f\x6f\x40\x62\x61\x72\x2e\x63\x6f\x6d"); // OLD, no phones
const std::string gBuf("\x0a\x05\x4b\x65\x76\x69\x6e\x10\x84\x86\x88\x08\x1a\x0b\x66\x6f\x6f\x40\x62\x61\x72\x2e\x63\x6f\x6d\x22\x0f\x0a\x0b\x2b\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x10\x00\x22\x0f\x0a\x0b\x2b\x30\x39\x38\x37\x36\x35\x34\x33\x32\x31\x10\x01", 59); // NEW, with 2 phones

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
