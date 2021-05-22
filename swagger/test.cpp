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
    get_enum_response_v
    get_enum_i(
        asio_http::asio::io_service&   aService,
        const get_enum_parameters&     aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        get_enum_response_200 sResult;
        sResult.body.push_back("one");
        sResult.body.push_back("two");
        return sResult;
    }

    get_status_response_v
    get_status_i(
        asio_http::asio::io_service&   aService,
        const get_status_parameters&   aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        get_status_response_200 sResult;
        sResult.body.load   = 1.5;
        sResult.body.status = "ready";
        return sResult;
    }
    virtual ~Common() {}
};

struct KeyValue : api::keyValue_1_0::server
{
    struct data
    {
        int16_t     version = {};
        std::string value;
    };
    std::map<std::string, data> m_Store;

    get_kv_key_response_v
    get_kv_key_i(
        asio_http::asio::io_service&   aService,
        const get_kv_key_parameters&   aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return boost::beast::http::status::not_found;
        return get_kv_key_response_200{sIt->second.value, sIt->second.version};
    }

    put_kv_key_response_v
    put_kv_key_i(
        asio_http::asio::io_service&   aService,
        const put_kv_key_parameters&   aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        // atomic update
        if (aRequest.if_match.has_value()) {
            auto sIt = m_Store.find(aRequest.key.value());
            if (sIt == m_Store.end())
                return put_kv_key_response_412{};
            if (sIt->second.version != aRequest.if_match.value())
                return put_kv_key_response_412{};
            sIt->second.value = aRequest.body;
            sIt->second.version++;
            return put_kv_key_response_200{};
        }

        // atomic insert
        if (aRequest.if_none_match.has_value())
        {
            if (aRequest.if_none_match.value() != "*")
                return put_kv_key_response_412{};
            auto sIt = m_Store.find(aRequest.key.value());
            if (sIt != m_Store.end())
                return put_kv_key_response_412{};
            m_Store[aRequest.key.value()] = data{1, aRequest.body};
            return put_kv_key_response_201{};
        }

        // unconditional update or insert
        auto& sValue = m_Store[aRequest.key.value()];
        const bool sInsert = sValue.version == 0;
        sValue.value = aRequest.body;
        sValue.version++;

        if (sInsert)
            return put_kv_key_response_201{};
        else
            return put_kv_key_response_200{};
    }

    delete_kv_key_response_v
    delete_kv_key_i(
        asio_http::asio::io_service&    aService,
        const delete_kv_key_parameters& aRequest,
        asio_http::asio::yield_context  yield)
        override
    {
        // atomic delete
        if (aRequest.if_match.has_value()) {
            auto sIt = m_Store.find(aRequest.key.value());
            if (sIt == m_Store.end())
                return delete_kv_key_response_412{};
            if (sIt->second.version != aRequest.if_match.value())
                return delete_kv_key_response_412{};
            m_Store.erase(sIt);
            return delete_kv_key_response_200{};
        }

        // unconditional delete
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return boost::beast::http::status::not_found;
        m_Store.erase(sIt);
        return boost::beast::http::status::ok;
    }

    head_kv_key_response_v
    head_kv_key_i(
        asio_http::asio::io_service&   aService,
        const head_kv_key_parameters&  aRequest,
        asio_http::asio::yield_context yield)
        override
    {
        auto sIt = m_Store.find(aRequest.key.value());
        if (sIt == m_Store.end())
            return boost::beast::http::status::not_found;
        return boost::beast::http::status::ok;
    }

    virtual ~KeyValue() {}
};

struct jsonParam : api::jsonParam_1_0::server
{
    get_test1_response_v
    get_test1_i(asio_http::asio::io_service&   aService,
                const get_test1_parameters&    aRequest,
                asio_http::asio::yield_context yield) override
    {
        return get_test1_response_200{swagger::format(aRequest.param)};
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

        auto                           sResponse = sClient.get_enum({});
        const std::vector<std::string> sExpected = {{"one"}, {"two"}};
        BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(), sExpected.end(), sResponse.body.begin(), sResponse.body.end());
    }

    // keyValue with generated client
    {
        api::keyValue_1_0::client sClient(sAsio, "http://127.0.0.1:3000");

        auto R1 = sClient.put_kv_key({.key = "abc", .body = "abc_data"});
        std::get<api::keyValue_1_0::put_kv_key_response_201>(R1);

        // get
        int16_t sVersion = 0;
        auto    R2       = sClient.get_kv_key({.key = "abc"});
        BOOST_CHECK_EQUAL(R2.body, "abc_data");
        sVersion = R2.etag.value();

        // atomic update
        auto R3 = sClient.put_kv_key({.key = "abc", .if_match = sVersion, .body="abc_update1"});
        std::get<api::keyValue_1_0::put_kv_key_response_200>(R3);

        // atomic update with bad version
        try {
            sClient.put_kv_key({.key = "abc", .if_match = sVersion, .body="abc_update2"});
            BOOST_TEST(false); // must not happen
        } catch (api::keyValue_1_0::put_kv_key_response_412& e) {
            BOOST_TEST(true, "http error 412 occured as expected");
        }

        // atomic delete with proper version
        sVersion++;
        sClient.delete_kv_key({.key = "abc", .if_match = sVersion});

        // call not implemented method
        BOOST_CHECK_THROW(sClient.get_kx_multi({}), Exception::HttpError);
    }

    // json param
    {
        api::jsonParam_1_0::client sClient(sAsio, "http://127.0.0.1:3000");

        auto R1 = sClient.get_test1({.param = {{"one", true}, {"two", false}}});
        BOOST_TEST_MESSAGE("json response: " << R1.body);
    }
}
BOOST_AUTO_TEST_SUITE_END()
