#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Client.hpp"
#include "Server.hpp"

using namespace AsioHttp;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(kv)
BOOST_AUTO_TEST_CASE(basic)
{
    ba::io_service sAsio;
    auto           sHttpServer = createServer({});
    auto           sKvServer   = std::make_shared<KV::Server>();
    sKvServer->Configure(sHttpServer);

    bool sOk = false;
    ba::co_spawn(sAsio, [&]() -> ba::awaitable<void> { co_return co_await sHttpServer->run(); }, ba::detached);
    ba::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            KV::Client   sClient;
            KV::Requests sRequests;
            sRequests.push_back({.key = "123", .value = "foo"});
            sRequests.push_back({.key = "124", .value = "bar"});
            auto sResponses = co_await sClient.Call(sRequests);
            BOOST_CHECK_EQUAL(sResponses.size(), 2);
            for (auto& x : sResponses) {
                BOOST_TEST_MESSAGE("set key " << x.key);
            }

            sRequests.clear();
            sRequests.push_back({.key = "123"});
            sRequests.push_back({.key = "124"});
            sRequests.push_back({.key = "125"});
            sResponses = co_await sClient.Call(sRequests);
            for (auto& x : sResponses) {
                BOOST_TEST_MESSAGE("get key " << x.key << " = " << x.value.value_or("[not found]"));
            }
            sOk = sResponses.size() == 3;
        },
        ba::detached);
    sAsio.run_for(100ms);
    BOOST_CHECK(sOk);
}

BOOST_AUTO_TEST_CASE(batch)
{
    ba::io_service sAsio;
    auto           sHttpServer = createServer({});
    auto           sKvServer   = std::make_shared<KV::Server>();
    sKvServer->Configure(sHttpServer);

    auto sClient = std::make_shared<KV::BatchClient>(KV::Params{.batch_size = 30});

    constexpr unsigned COUNT = 100;
    unsigned           sOk   = 0;
    ba::co_spawn(sAsio, [&]() -> ba::awaitable<void> { co_return co_await sHttpServer->run(); }, ba::detached);

    for (unsigned i = 0; i < COUNT; i++) {
        ba::co_spawn(
            sAsio,
            [&, sId = i]() -> ba::awaitable<void> {
                // set
                KV::Request sRequest{.key = std::to_string(sId), .value = std::to_string(sId * 10)};
                auto        sResponse = co_await sClient->Call(sRequest);

                // get
                sRequest.value = {};
                sResponse      = co_await sClient->Call(sRequest);

                // check
                BOOST_CHECK_EQUAL(std::to_string(sId * 10), sResponse.value.value());
                sOk++;
            },
            ba::detached);
    }
    sAsio.run_for(200ms);
    BOOST_CHECK_EQUAL(sOk, COUNT);
}
BOOST_AUTO_TEST_SUITE_END()
