#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Alive.hpp"
#include "Client.hpp"
#include "Router.hpp"
#include "Server.hpp"
#include "v2/Client.hpp"
#include "v2/Server.hpp"

#include <curl/Curl.hpp>
using namespace std::chrono_literals;

#include <time/Meter.hpp>

BOOST_AUTO_TEST_SUITE(asio_http)
BOOST_AUTO_TEST_CASE(simple)
{
    unsigned sSerial = 0;
    auto     sRouter = std::make_shared<asio_http::Router>();
    sRouter->insert("/hello", [&sSerial](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
        if (aRequest.method() != http::verb::get) {
            aResponse.result(http::status::method_not_allowed);
            return;
        }
        sSerial++;

        for (auto& x : aRequest)
            BOOST_TEST_MESSAGE(x.name() << " = " << x.value());

        if (sSerial == 1)
            BOOST_CHECK_EQUAL(aRequest["User-Agent"], "Curl++");
        if (sSerial == 2)
            BOOST_CHECK_EQUAL(aRequest["User-Agent"], "Beast/cxx");

        aResponse.result(http::status::ok);
        aResponse.set(asio_http::Headers::ContentType, "text/html");
        aResponse.body() = "hello world";
    });

    Threads::Asio  sAsio;
    Threads::Group sGroup;
    asio_http::startServer(sAsio, 2081, sRouter);
    sAsio.start(sGroup);

    std::this_thread::sleep_for(100ms);

    Curl::Client sClient;
    auto         sResult = sClient.GET("http://127.0.0.1:2081/hello");
    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_CHECK_EQUAL(sResult.body, "hello world");

    sResult = sClient.POST("http://127.0.0.1:2081/hello", "data");
    BOOST_CHECK_EQUAL(sResult.status, 405);

    sResult = sClient.GET("http://127.0.0.1:2081/other");
    BOOST_CHECK_EQUAL(sResult.status, 404);

    // beast client
    asio_http::ClientRequest sRequest{.method = http::verb::get, .url = "http://127.0.0.1:2081/hello", .headers = {{asio_http::Headers::Host, "127.0.0.1"}, {asio_http::Headers::UserAgent, "Beast/cxx"}}};

    auto sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "hello world");

    BOOST_REQUIRE_EQUAL(sSerial, 2); // all 2 requests processed
}
BOOST_AUTO_TEST_CASE(alive)
{
    unsigned sSerial = 0;
    auto     sRouter = std::make_shared<asio_http::Router>();
    sRouter->insert("/hello", [&sSerial](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
        if (aRequest.method() != http::verb::get) {
            aResponse.result(http::status::method_not_allowed);
            return;
        }
        sSerial++;
        aResponse.result(http::status::ok);
        aResponse.set(asio_http::Headers::ContentType, "text/html");
        aResponse.body() = std::to_string(sSerial);
    });
    sRouter->insert("/slow", [&sSerial](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
        sleep(1);
        aResponse.result(http::status::ok);
    });

    Threads::Asio  sAsio;
    Threads::Group sGroup;
    asio_http::startServer(sAsio, 2081, sRouter);
    sAsio.start(sGroup, 2);

    asio_http::ClientRequest sRequest{.method = http::verb::get, .url = "http://127.0.0.1:2081/hello", .headers = {{asio_http::Headers::Host, "127.0.0.1"}}};

    asio_http::Alive::Params sParams;
    auto                     sManager = std::make_shared<asio_http::Alive::Manager>(sAsio.service(), sParams);
    sManager->start_cleaner();
    auto sResponse = sManager->async(std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "1");

    sleep(1);

    sRequest  = {.method = http::verb::get, .url = "http://127.0.0.1:2081/hello", .headers = {{asio_http::Headers::Host, "127.0.0.1"}}};
    sResponse = sManager->async(std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "2");
    BOOST_REQUIRE_EQUAL(sSerial, 2); // all requests processed

    // request timeout over keep alive connection
    // set 500ms timeout and server will reply after 1s.
    Time::Meter sMeter;
    sRequest     = {.method = http::verb::get, .url = "http://127.0.0.1:2081/slow", .headers = {{asio_http::Headers::Host, "127.0.0.1"}}, .total = 500};
    auto sFuture = sManager->async(std::move(sRequest));
    BOOST_CHECK_THROW(sFuture.get(), std::runtime_error);
    BOOST_CHECK_CLOSE(sMeter.get().to_double(), 0.5, 1);

    // async request from coro
    sAsio.spawn([&sManager](boost::asio::yield_context yield) {
        BOOST_TEST_MESSAGE("coro started");
        asio_http::ClientRequest sRequest{.method  = http::verb::get,
                                          .url     = "http://127.0.0.1:2081/hello",
                                          .headers = {{asio_http::Headers::Host, "127.0.0.1"}}};
        BOOST_TEST_MESSAGE("async started");
        auto sCompletion = sManager->async(std::move(sRequest), yield);
        BOOST_TEST_MESSAGE("wait ...");
        auto sResponse = sCompletion->result.get()->get_future().get();
        BOOST_TEST_MESSAGE("got result");
        BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    });
    sleep(1);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(asio_http_v2)
BOOST_AUTO_TEST_CASE(server2)
{
    auto sRouter = std::make_shared<asio_http::Router>();
    sRouter->insert("/hello", [](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
        if (aRequest.method() == asio_http::http::verb::post)
            BOOST_TEST_MESSAGE("post with body size " << aRequest.body().size());
        aResponse.result(asio_http::http::status::ok);
        aResponse.set(asio_http::Headers::ContentType, "text/html");
        while (aResponse.body().size() < 130 * 1024) {
            aResponse.body() += "hello world";
        }
    });
    Threads::Asio  sAsio;
    Threads::Group sGroup;
    asio_http::v2::startServer(sAsio, 2081, sRouter);
    sAsio.start(sGroup);
    std::this_thread::sleep_for(100ms);

    auto sPeer = std::make_shared<asio_http::v2::Peer>(sAsio.service(), "127.0.0.1", "2081");
    sPeer->start();
    std::this_thread::sleep_for(100ms);

    std::string sRequestBody;
    while (sRequestBody.size() < 130 * 1024)
        sRequestBody += "request body";
    auto sFuture = sPeer->async({.method  = asio_http::http::verb::post,
                                 .url     = "http://127.0.0.1:2081/hello",
                                 .body    = sRequestBody,
                                 .headers = {
                                     {asio_http::Headers::UserAgent, "TestAgent"}}});
    sFuture.wait();
    auto sResponse = sFuture.get();
    BOOST_TEST_MESSAGE("response status: " << sResponse.result());
    BOOST_TEST_MESSAGE("response body size: " << sResponse.body().size());
    for (auto& x : sResponse)
        BOOST_TEST_MESSAGE("\t" << x.name() << ": " << x.value());
}
BOOST_AUTO_TEST_CASE(server2_order)
{
    auto sRouter = std::make_shared<asio_http::Router>();
    sRouter->insert("/sleep1", [](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
        std::this_thread::sleep_for(1s);
        aResponse.result(asio_http::http::status::ok);
        aResponse.body() = "sleep 1 done";
    });
    sRouter->insert("/sleep2", [](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
        std::this_thread::sleep_for(2s);
        aResponse.result(asio_http::http::status::ok);
        aResponse.body() = "sleep 2 done";
    });
    Threads::Asio  sAsio;
    Threads::Group sGroup;
    asio_http::v2::startServer(sAsio, 2081, sRouter);
    sAsio.start(sGroup, 3); // 2 threads used for slow calls.
    std::this_thread::sleep_for(100ms);

    auto sPeer = std::make_shared<asio_http::v2::Peer>(sAsio.service(), "127.0.0.1", "2081");
    sPeer->start();
    std::this_thread::sleep_for(100ms);

    std::atomic_bool sOrder = false;
    std::thread sR1([&sPeer, &sOrder]() {
        auto sFuture = sPeer->async({.method  = asio_http::http::verb::get,
                                     .url     = "http://127.0.0.1:2081/sleep2",
                                     .headers = {
                                         {asio_http::Headers::UserAgent, "TestAgent"}}});
        sFuture.wait();
        auto sResponse = sFuture.get();
        BOOST_TEST_MESSAGE("response 1: " << sResponse.result() << " with body " << sResponse.body());
        BOOST_CHECK_MESSAGE(sOrder, "request order");
    });
    std::this_thread::sleep_for(100ms); // ensure 1st request started

    BOOST_TEST_MESSAGE("2nd call ...");
    std::thread sR2([&sPeer, &sOrder]() {
        auto sFuture = sPeer->async({.method  = asio_http::http::verb::get,
                                     .url     = "http://127.0.0.1:2081/sleep1",
                                     .headers = {
                                         {asio_http::Headers::UserAgent, "TestAgent"}}});
        sFuture.wait();
        auto sResponse = sFuture.get();
        BOOST_TEST_MESSAGE("response 2: " << sResponse.result() << " with body " << sResponse.body());
        sOrder = true;
    });

    BOOST_TEST_MESSAGE("wait ...");
    sR1.join();
    sR2.join();
}
BOOST_AUTO_TEST_SUITE_END()