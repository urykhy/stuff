#include <benchmark/benchmark.h>

#include <vector>

#include "tutorial.pb.h"

#include <Protobuf.hpp>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/util/json_util.h>

const bool gProtobufCleanup = []() { std::atexit(google::protobuf::ShutdownProtobufLibrary); return true; }();

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
        std::string      sTmp;
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
