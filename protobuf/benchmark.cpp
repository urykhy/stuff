#include <benchmark/benchmark.h>

#include <vector>

#include "tutorial.pb.h"

#include <Protobuf.hpp>

struct PhoneNumber
{
    std::string number;
    int         type = 1;

    void Clear()
    {
        number.clear();
        type = 1;
    }

    void ParseFromString(std::string_view aString)
    {
        Protobuf::Reader sReader(aString);
        sReader.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Reader* aReader) -> Protobuf::Action {
            switch (aField.id) {
            case 1: aReader->read(number); return Protobuf::ACT_USED;
            case 2: aReader->read(type); return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
        });
    }

    std::string SerializeAsString() const
    {
        std::string sTmp;
        Protobuf::Writer sWriter(sTmp);
        sWriter.write(1, number);
        sWriter.write(2, type);
        return sTmp;
    }
};

struct MyPerson
{
    std::string              name;
    std::string              email;
    int32_t                  id = 0;
    std::vector<PhoneNumber> phones;

    void Clear()
    {
        name.clear();
        email.clear();
        id = 0;
        phones.clear();
    }

    void ParseFromString(std::string_view aString)
    {
        Clear();
        Protobuf::Reader sReader(aString);
        sReader.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Reader* aReader) -> Protobuf::Action {
            switch (aField.id) {
            case 1: aReader->read(name); return Protobuf::ACT_USED;
            case 2: aReader->read(id); return Protobuf::ACT_USED;
            case 3: aReader->read(email); return Protobuf::ACT_USED;
            case 4: {
                std::string_view sTmpBuf;
                aReader->read(sTmpBuf);
                phones.push_back(PhoneNumber{});
                phones.back().ParseFromString(sTmpBuf);
            }
                return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
        });
    }

    std::string SerializeAsString() const
    {
        std::string sTmp;
        sTmp.reserve(128);
        Protobuf::Writer sWriter(sTmp);
        sWriter.write(1, name);
        sWriter.write(2, id);
        sWriter.write(3, email);
        for (auto& x : phones)
            sWriter.write(4, x.SerializeAsString());
        return sTmp;
    }
};

//const std::string gBuf("\x0a\x05\x4b\x65\x76\x69\x6e\x10\x84\x86\x88\x08\x1a\x0b\x66\x6f\x6f\x40\x62\x61\x72\x2e\x63\x6f\x6d"); // OLD, no phones
const std::string gBuf("\x0a\x05\x4b\x65\x76\x69\x6e\x10\x84\x86\x88\x08\x1a\x0b\x66\x6f\x6f\x40\x62\x61\x72\x2e\x63\x6f\x6d\x22\x0f\x0a\x0b\x2b\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x10\x00\x22\x0f\x0a\x0b\x2b\x30\x39\x38\x37\x36\x35\x34\x33\x32\x31\x10\x01", 59); // NEW, with 2 phones

static void BM_GoogleParse(benchmark::State& state)
{
    for (auto _ : state) {
        tutorial::Person sPerson;
        sPerson.ParseFromString(gBuf);
    }
}
BENCHMARK(BM_GoogleParse);

static void BM_GoogleParseReuse(benchmark::State& state)
{
    tutorial::Person sPerson;
    for (auto _ : state)
        sPerson.ParseFromString(gBuf);
}
BENCHMARK(BM_GoogleParseReuse);

static void BM_CustomParse(benchmark::State& state)
{
    MyPerson sMyPerson;
    for (auto _ : state)
        sMyPerson.ParseFromString(gBuf);
}
BENCHMARK(BM_CustomParse);

//

static void BM_GoogleSerialize(benchmark::State& state)
{
    tutorial::Person sPerson;
    sPerson.ParseFromString(gBuf);
    for (auto _ : state)
        benchmark::DoNotOptimize(sPerson.SerializeAsString());
}
BENCHMARK(BM_GoogleSerialize);

static void BM_CustomSerialize(benchmark::State& state)
{
    MyPerson sPerson;
    sPerson.ParseFromString(gBuf);
    for (auto _ : state)
        benchmark::DoNotOptimize(sPerson.SerializeAsString());
}
BENCHMARK(BM_CustomSerialize);

BENCHMARK_MAIN();
