#include <benchmark/benchmark.h>

#include <vector>

#include "Json.hpp"

#include <cbor/cbor-custom.hpp>
#include <cbor/decoder.hpp>
#include <cbor/encoder.hpp>

struct Tmp
{
    std::string base  = {};
    unsigned    index = {};

    void from_json(const ::Json::Value& aJson)
    {
        Parser::Json::from_value(aJson, "base", base);
        Parser::Json::from_value(aJson, "index", index);
    }
    void cbor_read(cbor::istream& sIn)
    {
        size_t sSize = cbor::get_uint(sIn, cbor::ensure_type(sIn, cbor::CBOR_LIST));
        assert(sSize == 2);
        cbor::read(sIn, base);
        cbor::read(sIn, index);
    }
    void cbor_write(cbor::ostream& sOut) const
    {
        cbor::write_type_value(sOut, cbor::CBOR_LIST, 2);
        cbor::write(sOut, base);
        cbor::write(sOut, index);
    }
};

const std::string gJsonStr = R"([{"base": "string1", "index": 123},{"base": "string2"}, {"base": "string3", "index": 125}])";

const std::string gCborStr = []() {
    std::vector<Tmp> sTmp;

    auto sJson = Parser::Json::parse(gJsonStr);
    Parser::Json::from_value(sJson, sTmp);
    cbor::omemstream sOut;
    cbor::write(sOut, sTmp);
    return sOut.str();
}();

static void BM_Json(benchmark::State& state)
{
    std::vector<Tmp> sTmp;
    for (auto _ : state) {
        auto sJson = Parser::Json::parse(gJsonStr);
        Parser::Json::from_value(sJson, sTmp);
        sTmp.clear();
    }
}
BENCHMARK(BM_Json);

static void BM_Cbor(benchmark::State& state)
{
    std::vector<Tmp> sTmp;
    for (auto _ : state) {
        cbor::imemstream sIn(gCborStr);
        cbor::read(sIn, sTmp);
        sTmp.clear();
    }
}
BENCHMARK(BM_Cbor);

BENCHMARK_MAIN();