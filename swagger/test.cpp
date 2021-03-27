#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <asio_http/Client.hpp>
#include <asio_http/Router.hpp>
#include <asio_http/Server.hpp>

#include <common.v1.hpp>
#include <keyValue.v1.hpp>

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
        sResult.base.push_back("one");
        sResult.base.push_back("two");
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

struct KeyValue : api::keyValue_1_0
{
    std::map<std::string, std::string> m_Store;

    std::pair<boost::beast::http::status, get_kv_key_response>
    get_kv_key_i(
        asio_http::asio::io_service&   aService,
        const get_kv_key_parameters&   aRequest,
        const get_kv_key_body&         aBody,
        asio_http::asio::yield_context yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return {boost::beast::http::status::not_found, {}};
        return {boost::beast::http::status::ok, {sIt->second}};
    }

    std::pair<boost::beast::http::status, put_kv_key_response>
    put_kv_key_i(
        asio_http::asio::io_service&   aService,
        const put_kv_key_parameters&   aRequest,
        const put_kv_key_body&         aBody,
        asio_http::asio::yield_context yield)
        override
    {
        auto [_, sInsert] = m_Store.insert_or_assign(aRequest.key.value(), aBody.body);
        return {sInsert ? boost::beast::http::status::created : boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, delete_kv_key_response>
    delete_kv_key_i(
        asio_http::asio::io_service&    aService,
        const delete_kv_key_parameters& aRequest,
        const delete_kv_key_body&       aBody,
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
        const head_kv_key_body&        aBody,
        asio_http::asio::yield_context yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return {boost::beast::http::status::not_found, {}};
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, get_kx_multi_response>
    get_kx_multi_i(asio_http::asio::io_service& aService,
        const get_kx_multi_parameters& aRequest,
        const get_kx_multi_body& aBody,
        asio_http::asio::yield_context yield)
        override
    {
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, put_kx_multi_response>
    put_kx_multi_i(boost::asio::io_service&,
        const put_kx_multi_parameters&,
        const put_kx_multi_body&,
        boost::asio::yield_context)
    override
    {
        return {boost::beast::http::status::ok, {}};
    }

    std::pair<boost::beast::http::status, delete_kx_multi_response>
    delete_kx_multi_i(boost::asio::io_service&,
        const delete_kx_multi_parameters&,
        const delete_kx_multi_body&,
        boost::asio::yield_context)
    override
    {
        return {boost::beast::http::status::ok, {}};
    }

    virtual ~KeyValue() {}
};

BOOST_AUTO_TEST_SUITE(rpc)
BOOST_AUTO_TEST_CASE(simple)
{
    auto   sRouter = std::make_shared<asio_http::Router>();
    Common sCommon;
    sCommon.configure(sRouter);

    KeyValue sKeyValue;
    sKeyValue.configure(sRouter);

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

    // part 2. key value
    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:2081/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::not_found);

    sRequest  = {.method = asio_http::http::verb::put, .url = "http://127.0.0.1:2081/api/v1/kv/123", .body = "test"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::created);

    sRequest  = {.method = asio_http::http::verb::put, .url = "http://127.0.0.1:2081/api/v1/kv/123", .body = "one more"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);

    sRequest  = {.method = asio_http::http::verb::get, .url = "http://127.0.0.1:2081/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body(), "one more");

    sRequest  = {.method = asio_http::http::verb::delete_, .url = "http://127.0.0.1:2081/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);

    sRequest  = {.method = asio_http::http::verb::delete_, .url = "http://127.0.0.1:2081/api/v1/kv/123"};
    sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::not_found);
}
BOOST_AUTO_TEST_SUITE_END()
