#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "common.v1.hpp"
#include "jsonParam.v1.hpp"
#include "keyValue.v1.hpp"
#include "tutorial.v1.hpp"

#include <asio_http/Client.hpp>
#include <asio_http/Router.hpp>
#include <asio_http/Server.hpp>
#include <format/Hex.hpp>
#include <resource/Get.hpp>
#include <resource/Server.hpp>

DECLARE_RESOURCE(swagger_ui_tar)

struct Common : api::common_1_0::server
{
    std::pair<boost::beast::http::status, get_enum_response>
    get_enum_i(
        asio_http::asio::io_service&   aService,
        const get_enum_parameters&     aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        get_enum_response sResult;
        sResult.body.push_back("one");
        sResult.body.push_back("two");
        return {boost::beast::http::status::ok, sResult};
    }

    std::pair<boost::beast::http::status, get_status_response>
    get_status_i(
        asio_http::asio::io_service&   aService,
        const get_status_parameters&   aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        get_status_response sResult;
        sResult.body.load   = 1.5;
        sResult.body.status = "ready";
        return {boost::beast::http::status::ok, sResult};
    }
    virtual ~Common() {}
};

struct KeyValue : api::keyValue_1_0::server
{
    std::map<std::string, std::string> m_Store;

    std::pair<boost::beast::http::status, get_kv_key_response>
    get_kv_key_i(
        asio_http::asio::io_service&   aService,
        const get_kv_key_parameters&   aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return {boost::beast::http::status::not_found, {}};
        return {boost::beast::http::status::ok, {sIt->second, aRequest.if_modified_since}};
    }

    std::pair<boost::beast::http::status, put_kv_key_response>
    put_kv_key_i(
        asio_http::asio::io_service&   aService,
        const put_kv_key_parameters&   aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        auto [_, sInsert] = m_Store.insert_or_assign(aRequest.key.value(), aRequest.body);
        return {sInsert ? boost::beast::http::status::created : boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, delete_kv_key_response>
    delete_kv_key_i(
        asio_http::asio::io_service&    aService,
        const delete_kv_key_parameters& aRequest,
        asio_http::asio::yield_context  yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return {boost::beast::http::status::not_found, {}};
        m_Store.erase(sIt);
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, head_kv_key_response>
    head_kv_key_i(
        asio_http::asio::io_service&   aService,
        const head_kv_key_parameters&  aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return {boost::beast::http::status::not_found, {}};
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, get_kx_multi_response>
    get_kx_multi_i(asio_http::asio::io_service&   aService,
                   const get_kx_multi_parameters& aRequest,
                   asio_http::asio::yield_context yield)
        override
    {
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, put_kx_multi_response>
    put_kx_multi_i(boost::asio::io_service&,
                   const put_kx_multi_parameters&,
                   boost::asio::yield_context)
        override
    {
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, delete_kx_multi_response>
    delete_kx_multi_i(boost::asio::io_service&,
                      const delete_kx_multi_parameters&,
                      boost::asio::yield_context)
        override
    {
        return {boost::beast::http::status::ok, {}};
    }

    virtual ~KeyValue() {}
};

struct jsonParam : api::jsonParam_1_0::server
{
    virtual std::pair<boost::beast::http::status, get_test1_response>
    get_test1_i(asio_http::asio::io_service&   aService,
                const get_test1_parameters&    aRequest,
                asio_http::asio::yield_context yield) override
    {
        return {boost::beast::http::status::ok, {swagger::format(aRequest.param)}};
    }
};

BOOST_AUTO_TEST_SUITE(rpc)
BOOST_AUTO_TEST_CASE(simple)
{
    auto   sRouter = std::make_shared<asio_http::Router>();
    Common sCommon;
    sCommon.configure(sRouter);

    resource::Server sUI("/swagger/", resource::swagger_ui_tar());
    sUI.configure(sRouter);

    KeyValue sKeyValue;
    sKeyValue.configure(sRouter);

    jsonParam sJsonParam;
    sJsonParam.configure(sRouter);

    Threads::Asio sAsio;
    asio_http::startServer(sAsio, 3000, sRouter); // using same port as in seagger schema
    Threads::Group sGroup;
    sAsio.start(sGroup);

    asio_http::ClientRequest sRequest{.method = asio_http::http::verb::get, .url = "http://127.0.0.1:3000/api/v1/enum", .headers = {{asio_http::http::field::host, "127.0.0.1"}}};
    auto                     sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "[\n\t\"one\",\n\t\"two\"\n]");

    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:3000/api/v1/status", .headers = {{asio_http::http::field::host, "127.0.0.1"}}};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "{\n\t\"load\" : 1.5,\n\t\"status\" : \"ready\"\n}");

    // part 2. key value
    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:3000/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::not_found);

    sRequest  = {.method = asio_http::http::verb::put, .url = "http://127.0.0.1:3000/api/v1/kv/123", .body = "test"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::created);

    sRequest  = {.method = asio_http::http::verb::put, .url = "http://127.0.0.1:3000/api/v1/kv/123", .body = "one more"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);

    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:3000/api/v1/kv/123", .headers = {{asio_http::http::field::if_modified_since, "123"}}};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "one more");
    BOOST_CHECK_EQUAL(sResponse["x-timestamp"].to_string(), "123");

    sRequest  = {.method = asio_http::http::verb::delete_, .url = "http://127.0.0.1:3000/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);

    sRequest  = {.method = asio_http::http::verb::delete_, .url = "http://127.0.0.1:3000/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::not_found);

    // call with cbor:
    {
        asio_http::ClientRequest sRequest{.method  = asio_http::http::verb::get,
                                          .url     = "http://127.0.0.1:3000/api/v1/enum",
                                          .headers = {{asio_http::http::field::host, "127.0.0.1"}, {asio_http::http::field::accept, "application/cbor"}}};
        auto                     sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
        BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
        std::vector<std::string> sResult;
        cbor::from_string(sResponse.body(), sResult);
        const std::vector<std::string> sExpected{{"one"}, {"two"}};
        BOOST_CHECK_EQUAL_COLLECTIONS(sResult.begin(), sResult.end(), sExpected.begin(), sExpected.end());
    }

    // embedded swagger ui
    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:3000/swagger/index.html"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body().size(), 1425);

    // generated client
    {
        api::common_1_0::client sClient(sAsio, "http://127.0.0.1:3000");
        auto                    sResponse = sClient.get_enum({});
        BOOST_CHECK_EQUAL(sResponse.first, asio_http::http::status::ok);
        const std::vector<std::string> sExpected = {{"one"}, {"two"}};
        BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(), sExpected.end(), sResponse.second.body.begin(), sResponse.second.body.end());
    }
    {
        api::keyValue_1_0::client sClient(sAsio, "http://127.0.0.1:3000");
        BOOST_CHECK_EQUAL(sClient.put_kv_key({.key = "abc", .body = "abc_data"}).first, asio_http::http::status::created);
        BOOST_CHECK_EQUAL(sClient.get_kv_key({.key = "abc"}).second.body, "abc_data");
    }

    // json param
    {
        api::jsonParam_1_0::client sClient(sAsio, "http://127.0.0.1:3000");
        auto sResult = sClient.get_test1({
            .param = {{"one", true}, {"two", false}}
        });
        BOOST_TEST_MESSAGE("json response: " << sResult.second.body);
    }
}
BOOST_AUTO_TEST_SUITE_END()
