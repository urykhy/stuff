#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include "Server.hpp"
#include "Router.hpp"
#include "Client.hpp"

#include <curl/Curl.hpp>
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(asio_http)
BOOST_AUTO_TEST_CASE(simple)
{
    auto sRouter = std::make_shared<asio_http::Router>();
    sRouter->insert("/hello", [](const asio_http::Request& aRequest, asio_http::Response& aResponse)
    {
        if (aRequest.method() != http::verb::get)
        {
            aResponse.result(http::status::method_not_allowed);
            return;
        }

        aResponse.result(http::status::ok);
        aResponse.set(http::field::content_type, "text/html");
        aResponse.body() = "hello world";
    });

    Threads::Asio sAsio;
    Threads::Group sGroup;
    asio_http::startServer(sAsio, 2081, sRouter);
    sAsio.start(sGroup);

    std::this_thread::sleep_for(100ms);

    Curl::Client::Params sParams;
    Curl::Client sClient(sParams);
    auto sResult = sClient.GET("http://127.0.0.1:2081/hello");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "hello world");

    sResult = sClient.POST("http://127.0.0.1:2081/hello", "data");
    BOOST_CHECK_EQUAL(sResult.first, 405);

    sResult = sClient.GET("http://127.0.0.1:2081/other");
    BOOST_CHECK_EQUAL(sResult.first, 404);

    // beast client
    asio_http::ClientRequest sRequest{"127.0.0.1", "2081", {http::verb::get, "/hello", 10}}; // 10 = 1.0 protocol
    sRequest.request.set(http::field::host, "127.0.0.1");
    sRequest.request.set(http::field::user_agent, "Beast/cxx");
    auto sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "hello world");

}
BOOST_AUTO_TEST_SUITE_END()
