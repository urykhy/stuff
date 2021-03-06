#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <curl/Curl.hpp>

#include "Alive.hpp"
#include "Client.hpp"
#include "Router.hpp"
#include "Server.hpp"
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
            BOOST_CHECK_EQUAL(aRequest.at("User-Agent"), "Curl/Client");
        if (sSerial == 2)
            BOOST_CHECK_EQUAL(aRequest.at("User-Agent"), "Beast/cxx");

        aResponse.result(http::status::ok);
        aResponse.set(http::field::content_type, "text/html");
        aResponse.body() = "hello world";
    });

    Threads::Asio  sAsio;
    Threads::Group sGroup;
    asio_http::startServer(sAsio, 2081, sRouter);
    sAsio.start(sGroup);

    std::this_thread::sleep_for(100ms);

    Curl::Client::Params sParams;
    Curl::Client         sClient(sParams);
    auto                 sResult = sClient.GET("http://127.0.0.1:2081/hello");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "hello world");

    sResult = sClient.POST("http://127.0.0.1:2081/hello", "data");
    BOOST_CHECK_EQUAL(sResult.first, 405);

    sResult = sClient.GET("http://127.0.0.1:2081/other");
    BOOST_CHECK_EQUAL(sResult.first, 404);

    // beast client
    asio_http::ClientRequest sRequest{.method = http::verb::get, .url = "http://127.0.0.1:2081/hello", .headers = {{http::field::host, "127.0.0.1"}, {http::field::user_agent, "Beast/cxx"}}};

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
        aResponse.set(http::field::content_type, "text/html");
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

    asio_http::ClientRequest sRequest{.method = http::verb::get, .url = "http://127.0.0.1:2081/hello", .headers = {{http::field::host, "127.0.0.1"}}};

    asio_http::Alive::Params sParams;
    auto sManager  = std::make_shared<asio_http::Alive::Manager>(sAsio.service(), sParams);
    sManager->start_cleaner();
    auto sResponse = sManager->async(std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "1");

    sleep(1);

    sRequest  = {.method = http::verb::get, .url = "http://127.0.0.1:2081/hello", .headers = {{http::field::host, "127.0.0.1"}}};
    sResponse = sManager->async(std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "2");
    BOOST_REQUIRE_EQUAL(sSerial, 2); // all requests processed

    // request timeout over keep alive connection
    // set 500ms timeout and server will reply after 1s.
    Time::Meter sMeter;
    sRequest     = {.method = http::verb::get, .url = "http://127.0.0.1:2081/slow", .headers = {{http::field::host, "127.0.0.1"}}, .total = 500};
    auto sFuture = sManager->async(std::move(sRequest));
    BOOST_CHECK_THROW(sFuture.get(), std::runtime_error);
    BOOST_CHECK_CLOSE(sMeter.get().to_double(), 0.5, 1);
}
BOOST_AUTO_TEST_SUITE_END()
