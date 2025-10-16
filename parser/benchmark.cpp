#define RAPIDJSON_HAS_STDSTRING 1

#include <benchmark/benchmark.h>
#include <fmt/ranges.h>
#include <rapidjson/document.h>
#include <simdjson.h>
#include <yyjson.h>

#include <iostream>
#include <vector>

#include "Json.hpp"

#include <cbor/cbor.hpp>
#include <nlohmann/json.hpp>
#include <rapidjson/error/en.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include <glaze/glaze.hpp>
#pragma GCC diagnostic pop

struct Tmp
{
    std::string base  = {};
    unsigned    index = {};

    void from_json(const ::Json::Value& aJson)
    {
        Parser::Json::from_object(aJson, "base", base);
        Parser::Json::from_object(aJson, "index", index);
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

static_assert(glz::reflectable<Tmp>);
static_assert(glz::write_supported<Tmp, glz::BEVE>);
static_assert(glz::read_supported<Tmp, glz::BEVE>);

void from_json(const nlohmann::json& aJson, Tmp& aTmp)
{
    if (auto sIt = aJson.find("base"); sIt != aJson.end())
        sIt->get_to(aTmp.base);
    if (auto sIt = aJson.find("index"); sIt != aJson.end())
        sIt->get_to(aTmp.index);
}

void from_json(const simdjson::dom::object& aJson, Tmp& aTmp)
{
    if (auto sIt = aJson["base"]; !sIt.error())
        aTmp.base = sIt.get_string().value();
    if (auto sIt = aJson["index"]; !sIt.error())
        aTmp.index = sIt.get_uint64();
}

void from_json(simdjson::ondemand::object aJson, Tmp& aTmp)
{
    for (auto sField : aJson) {
        auto sKey   = sField.key().value();
        auto sValue = sField.value();
        if (sKey == "base") {
            sValue.get_string(aTmp.base);
        } else if (sKey == "index") {
            aTmp.index = sValue.get_uint64();
        }
    }
}

void from_json(const rapidjson::GenericValue<rapidjson::UTF8<>>& aJson, Tmp& aTmp)
{
    if (auto sIt = aJson.FindMember("base"); sIt != aJson.MemberEnd())
        aTmp.base = sIt->value.GetString();
    if (auto sIt = aJson.FindMember("index"); sIt != aJson.MemberEnd())
        aTmp.index = sIt->value.GetUint64();
}

void from_json(const glz::json_t& aJson, Tmp& aTmp)
{
    const auto& sObject = aJson.get_object();
    if (auto sIt = sObject.find("base"); sIt != sObject.end())
        aTmp.base = sIt->second.get<std::string>();
    if (auto sIt = sObject.find("index"); sIt != sObject.end())
        aTmp.index = sIt->second.get<double>();
}

void from_json(yyjson_val* aObject, Tmp& aTmp)
{
    yyjson_val*     sKey  = nullptr;
    yyjson_obj_iter sIter = yyjson_obj_iter_with(aObject);
    while ((sKey = yyjson_obj_iter_next(&sIter))) {
        auto             sVal = yyjson_obj_iter_get_val(sKey);
        std::string_view sKeyV{yyjson_get_str(sKey), yyjson_get_len(sKey)};

        if (sKeyV == "base" and yyjson_is_str(sVal)) {
            aTmp.base.assign(yyjson_get_str(sVal), yyjson_get_len(sVal));
        } else if (sKeyV == "index" and yyjson_is_int(sVal)) {
            aTmp.index = yyjson_get_uint(sVal);
        }
    }
}

namespace fmt {
    template <>
    struct formatter<Tmp> : formatter<std::string>
    {
        format_context::iterator format(const Tmp& aTmp, format_context& aCtx) const
        {
            std::stringstream sStr;
            sStr << "(base: " << aTmp.base << "; index: " << aTmp.index << ")";
            return formatter<std::string>::format(sStr.str(), aCtx);
        }
    };
} // namespace fmt

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

static void BM_Nlohmann(benchmark::State& state)
{
    std::vector<Tmp> sTmp;
    for (auto _ : state) {
        nlohmann::json sJson = nlohmann::json::parse(gJsonStr);
        sJson.get_to(sTmp);
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_Nlohmann);

static void BM_Simd(benchmark::State& state)
{
    using namespace simdjson;

    const simdjson::padded_string sPadded = gJsonStr;
    std::vector<Tmp>              sTmp;
    dom::parser                   sParser;
    for (auto _ : state) {
        dom::element sDoc = sParser.parse(sPadded);
        if (sDoc.is_array()) {
            for (const auto& x : sDoc) {
                if (x.is_object()) {
                    sTmp.emplace_back(Tmp());
                    from_json(x.get_object(), sTmp.back());
                }
            }
        }
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_Simd);

static void BM_SimdOnDemand(benchmark::State& state)
{
    using namespace simdjson;

    const simdjson::padded_string sPadded = gJsonStr;
    std::vector<Tmp>              sTmp;
    ondemand::parser              sParser;
    for (auto _ : state) {
        auto sDoc = sParser.iterate(sPadded);
        for (auto x : sDoc) {
            sTmp.emplace_back(Tmp());
            from_json(x.value().get_object(), sTmp.back());
        }
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_SimdOnDemand);

static void BM_Rapid(benchmark::State& state)
{
    using namespace rapidjson;

    std::vector<Tmp> sTmp;
    Document         sDoc;
    for (auto _ : state) {
        if (sDoc.Parse(gJsonStr).HasParseError()) {
            break;
        }
        if (sDoc.IsArray()) {
            for (unsigned i = 0; i < sDoc.Size(); i++) {
                const auto& sItem = sDoc[i];
                if (sItem.IsObject()) {
                    sTmp.emplace_back(Tmp());
                    from_json(sItem, sTmp.back());
                }
            }
        }
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_Rapid);

static void BM_YY(benchmark::State& state)
{
    std::vector<Tmp> sTmp;
    for (auto _ : state) {
        yyjson_doc* sDoc = yyjson_read(gJsonStr.data(), gJsonStr.size(), 0);
        if (sDoc == nullptr) {
            return;
        }
        yyjson_val* sRoot = yyjson_doc_get_root(sDoc);

        if (yyjson_is_arr(sRoot)) {
            for (unsigned i = 0; i < yyjson_arr_size(sRoot); i++) {
                auto sItem = yyjson_arr_get(sRoot, i);
                if (yyjson_is_obj(sItem)) {
                    sTmp.emplace_back(Tmp());
                    from_json(sItem, sTmp.back());
                }
            }
        }
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
        yyjson_doc_free(sDoc);
    }
}
BENCHMARK(BM_YY);

static void BM_Glaze(benchmark::State& state)
{
    std::vector<Tmp> sTmp;
    glz::json_t      sJson;
    for (auto _ : state) {
        if (glz::read_json(sJson, gJsonStr)) {
            break;
        }
        if (sJson.is_array()) {
            for (const auto& x : sJson.get_array()) {
                if (x.is_object()) {
                    sTmp.emplace_back(Tmp());
                    from_json(x, sTmp.back());
                }
            }
        }
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_Glaze);

static void BM_GlazeRef(benchmark::State& state)
{
    std::vector<Tmp> sTmp;
    for (auto _ : state) {
        if (glz::read_json(sTmp, gJsonStr)) {
            break;
        }
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_GlazeRef);

static void BM_GlazeBinary(benchmark::State& state)
{
    // prepare
    glz::json_t sJson;
    if (glz::read_json(sJson, gJsonStr)) {
        return;
    }
    std::string sBeve;
    if (glz::write_beve(sJson, sBeve)) {
        return;
    }

    std::vector<Tmp> sTmp;
    for (auto _ : state) {
        if (glz::read_beve(sJson, sBeve)) {
            break;
        }
        if (sJson.is_array()) {
            for (const auto& x : sJson.get_array()) {
                if (x.is_object()) {
                    sTmp.emplace_back(Tmp());
                    from_json(x, sTmp.back());
                }
            }
        }
        // fail with syntax_error
        /*if (auto ec = glz::read_beve(sTmp, sBeve); ec) {
            std::cout << glz::format_error(ec, sBeve) << std::endl;
            break;
        }*/
        // std::cout << fmt::format("parsed: {}", fmt::join(sTmp, ", ")) << std::endl;
        sTmp.clear();
    }
}
BENCHMARK(BM_GlazeBinary);

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
