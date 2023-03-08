#include <benchmark/benchmark.h>

#define BOOST_TEST_MODULE Suites
#include "v2/Parser.hpp"

// http2 session with small response
// via nghttp

// header + data
const std::string g_FromServer = []() {
    return Parser::from_hex("00005301040000000d887690aa69d29ae452a9a74a6b13015db02e0f5889a47e561cc58197000f6196e4593e9403ca681d8a080265403f704cdc0854c5a37f0f0d0231326c96e4593e9403ca681d8a080265403f7042b8d014c5a37f") + Parser::from_hex("00000c00010000000d48656c6c6f20776f726c640a");
}();

// headers only
const std::string g_FromClient = []() {
    return Parser::from_hex("00002c01250000000d0000000b0f8204856272d141ff86418aa0e41d139d09b8203c1f53032a2f2a907a8aaa69d29ac4c0576c0b83");
}();

namespace parser = asio_http::v2::parser;

struct DummyParser : parser::API
{
    parser::Main m_Parser;
    parser::Mode m_Mode;

    // API
    parser::MessagePtr new_message(uint32_t aID) override
    {
        if (m_Mode == parser::SERVER)
            return std::make_shared<parser::AsioRequest>();
        else
            return std::make_shared<parser::AsioResponse>();
    }
    void process_message(uint32_t aID, parser::MessagePtr&& aMessage) override {}
    void window_update(uint32_t aID, uint32_t aInc) override {}
    void send(std::string&& aBuffer) override {}
    // ---

    DummyParser(parser::Mode aMode)
    : m_Parser(aMode, this)
    , m_Mode(aMode)
    {
        m_Parser.m_Stage = parser::Main::HEADER;
    }

    void parse(std::string_view aInput)
    {
        std::string sTmp;
        while (!aInput.empty()) {
            size_t sSize = m_Parser.hint();
            sTmp.assign(aInput.substr(0, sSize));
            aInput.remove_prefix(sSize);
            m_Parser.process(sTmp);
        }
    }
};

static void BM_FromClient(benchmark::State& state)
{
    DummyParser sParser(parser::SERVER);
    for (auto _ : state) {
        sParser.parse(g_FromClient);
    }
}
BENCHMARK(BM_FromClient);

static void BM_FromServer(benchmark::State& state)
{
    DummyParser sParser(parser::CLIENT);
    for (auto _ : state) {
        sParser.parse(g_FromServer);
    }
}
BENCHMARK(BM_FromServer);

BENCHMARK_MAIN();
