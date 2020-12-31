#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <asio_http/Client.hpp>
#include <asio_http/Router.hpp>
#include <asio_http/Server.hpp>

#include <common.v1.hpp>

struct Common : api::common_1_0
{
    std::pair<boost::beast::http::status, get_enum_response>
    get_enum_i(
        asio_http::asio::io_service&   aService,
        const get_enum_parameters&     aRequest,
        const get_enum_body&           aBody,
        asio_http::asio::yield_context yield)
        override
    {
        get_enum_response sResult;
        sResult.response.push_back("one");
        sResult.response.push_back("two");
        return {boost::beast::http::status::ok, sResult};
    }

    std::pair<boost::beast::http::status, get_status_response>
    get_status_i(
        asio_http::asio::io_service&   aService,
        const get_status_parameters&   aRequest,
        const get_status_body&         aBody,
        asio_http::asio::yield_context yield)
        override
    {
        get_status_response sResult;
        sResult.load   = 1.5;
        sResult.status = "ready";
        return {boost::beast::http::status::ok, sResult};
    }
    virtual ~Common() {}
};

BOOST_AUTO_TEST_SUITE(rpc)
BOOST_AUTO_TEST_CASE(simple)
{
    auto   sRouter = std::make_shared<asio_http::Router>();
    Common sCommon;
    sCommon.configure(sRouter);

    Threads::Asio sAsio;
    asio_http::startServer(sAsio, 2081, sRouter);
    Threads::Group sGroup;
    sAsio.start(sGroup);

    asio_http::ClientRequest sRequest{.method = asio_http::http::verb::get, .url = "http://127.0.0.1:2081/api/v1/enum", .headers = {{asio_http::http::field::host, "127.0.0.1"}}};
    auto                     sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "[\n\t\"one\",\n\t\"two\"\n]");

    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:2081/api/v1/status", .headers = {{asio_http::http::field::host, "127.0.0.1"}}};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "{\n\t\"load\" : 1.5,\n\t\"status\" : \"ready\"\n}");
}
BOOST_AUTO_TEST_SUITE_END()
