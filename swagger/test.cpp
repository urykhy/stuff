#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#define ASIO_HTTP_LIBRARY_HEADER

#include "MetricsDiscovery.hpp"
#include "common.v1.hpp"
#include "discovery.v1.hpp"
#include "jsonParam.v1.hpp"
#include "keyValue.v1.hpp"
#include "redirect.v1.hpp"
#include "tutorial.v1.hpp"

#include <asio_http/Client.hpp>
#include <asio_http/Server.hpp>
#include <asio_http/v2/Client.hpp>
#include <asio_http/v2/Server.hpp>
#include <format/Hex.hpp>
#include <jaeger/Client.hpp>
#include <jwt/JWT.hpp>
#include <prometheus/API.hpp>
#include <resource/Get.hpp>
#include <resource/Server.hpp>
#include <sd/Balancer.hpp>
#include <sd/NotifyWeight.hpp>
#include <sentry/Client.hpp>

DECLARE_RESOURCE(swagger_ui_tar, "swagger_ui.tar")

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

struct Discovery : api::discovery_1_0::server
{
    get_discovery_response_v
    get_discovery_i(asio_http::asio::io_service&    aService,
                    const get_discovery_parameters& aRequest,
                    asio_http::asio::yield_context  yield) override
    {
        get_discovery_response_200 sResult;
        sResult.body = "success";
        return sResult;
    }
};

struct Discovery500 : api::discovery_1_0::server
{
    get_discovery_response_v
    get_discovery_i(asio_http::asio::io_service&    aService,
                    const get_discovery_parameters& aRequest,
                    asio_http::asio::yield_context  yield) override
    {
        return boost::beast::http::status::internal_server_error;
    }
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

    bool
    get_kv_key_auth(const Jwt::Claim&            aClaim,
                    const get_kv_key_parameters& aRequest)
        override
    {
        BOOST_TEST_MESSAGE("get auth " << aClaim.iss << "/" << aClaim.aud << " for " << aRequest.key.value());
        return true;
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
        if (aRequest.if_none_match.has_value()) {
            if (aRequest.if_none_match.value() != "*")
                return put_kv_key_response_412{};
            auto sIt = m_Store.find(aRequest.key.value());
            if (sIt != m_Store.end())
                return put_kv_key_response_412{};
            m_Store[aRequest.key.value()] = data{1, aRequest.body};
            return put_kv_key_response_201{};
        }

        // unconditional update or insert
        auto&      sValue  = m_Store[aRequest.key.value()];
        const bool sInsert = sValue.version == 0;
        sValue.value       = aRequest.body;
        sValue.version++;

        if (sInsert)
            return put_kv_key_response_201{};
        else
            return put_kv_key_response_200{};
    }

    bool
    put_kv_key_auth(const Jwt::Claim&            aClaim,
                    const put_kv_key_parameters& aRequest)
        override
    {
        BOOST_TEST_MESSAGE("put auth " << aClaim.iss << "/" << aClaim.aud << " for " << aRequest.key.value());
        return true;
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

    bool
    delete_kv_key_auth(const Jwt::Claim&               aClaim,
                       const delete_kv_key_parameters& aRequest)
        override
    {
        BOOST_TEST_MESSAGE("del auth " << aClaim.iss << "/" << aClaim.aud << " for " << aRequest.key.value());
        return true;
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

struct TutorialServer : api::tutorial_1_0::server
{
    virtual get_parameters_response_v
    get_parameters_i(asio_http::asio::io_service&     aService,
                     const get_parameters_parameters& aRequest,
                     asio_http::asio::yield_context   yield,
                     std::optional<Jaeger::Span>&     aTrace)
    {
        {
            auto __span = Jaeger::Helper::start(aTrace, "prepare");
            std::this_thread::sleep_for(100ms);
        }
        {
            auto __span = Jaeger::Helper::start(aTrace, "fetch");
            std::this_thread::sleep_for(200ms);
        }
        {
            auto __span = Jaeger::Helper::start(aTrace, "merge");
            std::this_thread::sleep_for(100ms);
        }
        {
            auto __span = Jaeger::Helper::start(aTrace, "write");
            {
                auto __span1 = Jaeger::Helper::start(__span, "mysql");
                std::this_thread::sleep_for(300ms);
            }
            {
                auto __span1 = Jaeger::Helper::start(__span, "s3");
                std::this_thread::sleep_for(300ms);
            }
        }
        return get_parameters_response_200{.body = "success"};
    }
};

struct RedirectServer : api::redirect_1_0::server
{
    get_temporary_1_response_v
    get_temporary_1_i(asio_http::asio::io_service&      aService,
                      const get_temporary_1_parameters& aRequest,
                      asio_http::asio::yield_context    yield)
        override
    {
        return get_temporary_1_response_302{.location = "http://127.0.0.1:3000/api/v1/redirect/temporary_2"};
    }
    get_temporary_2_response_v
    get_temporary_2_i(asio_http::asio::io_service&      aService,
                      const get_temporary_2_parameters& aRequest,
                      asio_http::asio::yield_context    yield)
        override
    {
        return get_temporary_2_response_307{.location = "http://127.0.0.1:3000/api/v1/redirect/finish"};
    }
    get_permanent_response_v
    get_permanent_i(asio_http::asio::io_service&    aService,
                    const get_permanent_parameters& aRequest,
                    asio_http::asio::yield_context  yield)
        override
    {
        return get_permanent_response_308{.location = "http://127.0.0.1:3001/api/v1/redirect/finish"};
    }
    get_finish_response_v
    get_finish_i(asio_http::asio::io_service&   aService,
                 const get_finish_parameters&   aRequest,
                 asio_http::asio::yield_context yield)
        override
    {
        return get_finish_response_200{.body = "success"};
    }
};

struct RedirectServerX : api::redirect_1_0::server
{
    get_permanent_response_v
    get_permanent_i(asio_http::asio::io_service&    aService,
                    const get_permanent_parameters& aRequest,
                    asio_http::asio::yield_context  yield)
        override
    {
        return get_permanent_response_200{.body = "success"};
    }
    get_finish_response_v
    get_finish_i(asio_http::asio::io_service&   aService,
                 const get_finish_parameters&   aRequest,
                 asio_http::asio::yield_context yield)
        override
    {
        return get_finish_response_200{.body = "success"};
    }
};

struct WithServer
{
    Threads::Asio                      m_Asio;
    std::shared_ptr<asio_http::Router> m_Router;
    std::shared_ptr<asio_http::Client> m_HttpClient;
    Threads::Group                     m_Group;

    WithServer()
    {
        m_Router     = std::make_shared<asio_http::Router>();
        m_HttpClient = asio_http::v1::makeClient(m_Asio.service());
        asio_http::startServer(m_Asio.service(), 3000, m_Router);
        m_Asio.start(m_Group);
    }
};

struct WithServerV2
{
    Threads::Asio                      m_Asio;
    std::shared_ptr<asio_http::Router> m_Router;
    std::shared_ptr<asio_http::Client> m_HttpClient;
    Threads::Group                     m_Group;

    WithServerV2()
    {
        m_Router     = std::make_shared<asio_http::Router>();
        m_HttpClient = asio_http::v2::makeClient(m_Asio.service());
        asio_http::v2::startServer(m_Asio.service(), 3000, m_Router);
        m_Asio.start(m_Group);
    }
};

BOOST_AUTO_TEST_SUITE(rpc)
BOOST_FIXTURE_TEST_CASE(simple, WithServer)
{
    Common sCommon;
    sCommon.configure(m_Router);

    api::common_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    auto sResponse = sClient.get_enum({});

    const std::vector<std::string> sExpected = {{"one"}, {"two"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(),
                                  sExpected.end(),
                                  sResponse.body.begin(),
                                  sResponse.body.end());

    Prometheus::Manager::instance().onTimer();
    for (auto& x : Prometheus::Manager::instance().toPrometheus())
        BOOST_TEST_MESSAGE(x);
}
BOOST_FIXTURE_TEST_CASE(kv_with_auth, WithServer)
{
    const Jwt::HS256 sTokenManager("secret");
    KeyValue         sKeyValue;
    sKeyValue.configure(m_Router);
    sKeyValue.with_authorization(&sTokenManager);

    api::keyValue_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    Jwt::Claim sClaim{.exp = time(nullptr) + 10,
                      .nbf = time(nullptr) - 10,
                      .iss = "test",
                      .aud = "kv"};
    sClient.__with_token(sTokenManager.Sign(sClaim));

    auto R1 = sClient.put_kv_key({.key = "abc", .body = "abc_data"});
    std::get<api::keyValue_1_0::put_kv_key_response_201>(R1);

    // get
    int16_t sVersion = 0;
    auto    R2       = sClient.get_kv_key({.key = "abc"});
    BOOST_CHECK_EQUAL(R2.body, "abc_data");
    sVersion = R2.etag.value();

    // atomic update
    auto R3 = sClient.put_kv_key({.key = "abc", .if_match = sVersion, .body = "abc_update1"});
    std::get<api::keyValue_1_0::put_kv_key_response_200>(R3);

    // atomic update with bad version
    try {
        sClient.put_kv_key({.key = "abc", .if_match = sVersion, .body = "abc_update2"});
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
BOOST_FIXTURE_TEST_CASE(swagger_ui, WithServer)
{
    resource::Server sUI("/swagger/", resource::swagger_ui_tar());
    sUI.configure(m_Router);

    asio_http::ClientRequest sRequest = {
        .method = asio_http::http::verb::get,
        .url    = "http://127.0.0.1:3000/swagger/index.html"};

    auto sResponse = asio_http::async(m_Asio.service(), std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_CHECK_EQUAL(sResponse.body().size(), 1425);
}
BOOST_FIXTURE_TEST_CASE(json, WithServer)
{
    jsonParam sJsonParam;
    sJsonParam.configure(m_Router);

    api::jsonParam_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    auto R1 = sClient.get_test1({.param = {{"one", true}, {"two", false}}});
    BOOST_TEST_MESSAGE("json response: " << R1.body);
}
BOOST_FIXTURE_TEST_CASE(sentry, WithServer)
{
    Sentry::Queue sQueue;
    sQueue.start();

    TutorialServer sTutorialServer;
    sTutorialServer.with_sentry([&sQueue](Sentry::Message& aMessage) { sQueue.send(aMessage); }, 1);
    sTutorialServer.configure(m_Router);
    Util::Raii sCleanup([this]() { m_Group.wait(); });

    api::tutorial_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");
    try {
        sClient.get_parameters({.id = "test-id", .x_header_int = 92});
    } catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("catch: " << e.what());
    }
}
BOOST_FIXTURE_TEST_CASE(queue, WithServer)
{
    Threads::QueueExecutor sTaskQueue;
    sTaskQueue.start(m_Group, 4); // spawn 4 threads

    TutorialServer sTutorialServer;
    sTutorialServer.with_queue(sTaskQueue);
    sTutorialServer.configure(m_Router);
    Util::Raii sCleanup([this]() { m_Group.wait(); });

    api::tutorial_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    Time::Meter sMeter;
    std::thread sR1([&sClient]() {
        try {
            auto sR = sClient.get_parameters({.id = "test-id", .string_required = "abcdefg"});
            BOOST_TEST_MESSAGE("request1: " << sR.body);
        } catch (const std::exception& e) {
            BOOST_TEST_MESSAGE("got exception: " << e.what());
        }
    });
    Threads::sleep(0.1);
    std::thread sR2([&sClient]() {
        try {
            auto sR = sClient.get_parameters({.id = "test-id", .string_required = "abcdefh"});
            BOOST_TEST_MESSAGE("request2: " << sR.body);
        } catch (const std::exception& e) {
            BOOST_TEST_MESSAGE("got exception: " << e.what());
        }
    });
    sR1.join();
    sR2.join();
    BOOST_CHECK_CLOSE(sMeter.get().to_double(), 1.1, 5); // 5% difference is ok
}
BOOST_FIXTURE_TEST_CASE(jaeger, WithServer)
{
    Jaeger::Queue sQueue("swagger.cpp", "0.1");
    sQueue.start();

    Threads::QueueExecutor sTaskQueue;
    sTaskQueue.start(m_Group, 4); // spawn 4 threads

    TutorialServer sTutorialServer;
    sTutorialServer.with_queue(sTaskQueue);
    sTutorialServer.with_jaeger([&sQueue](Jaeger::Trace& aTrace) { sQueue.send(aTrace); });
    sTutorialServer.configure(m_Router);
    Util::Raii sCleanup([this]() { m_Group.wait(); });

    api::tutorial_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    Jaeger::Trace sTrace(Jaeger::Params::uuid());
    {
        Jaeger::Span sTraceSpan(sTrace, "make test");

        auto sR = sClient.get_parameters({.id = "test-id", .string_required = "abcdefg"}, &sTraceSpan);
        BOOST_TEST_MESSAGE("request: " << sR.body);
    }
    sQueue.send(sTrace);
}
BOOST_FIXTURE_TEST_CASE(redirect, WithServer)
{
    RedirectServer sRedirectServer;
    sRedirectServer.configure(m_Router);

    api::redirect_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    // temporary
    {
        auto sR = sClient.get_temporary_1({});
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, api::redirect_1_0::server::get_temporary_1_response_200>) {
                BOOST_CHECK_EQUAL(arg.body, "success");
            }
        },
                   sR);
    }

    // permanent
    // redirect from server at 3000 to serverX at 3001
    {
        RedirectServerX                    sRedirectServerX;
        std::shared_ptr<asio_http::Router> sRouter = std::make_shared<asio_http::Router>();
        sRedirectServerX.configure(sRouter);
        asio_http::startServer(m_Asio.service(), 3001, sRouter);

        auto sR = sClient.get_permanent({});
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, api::redirect_1_0::server::get_permanent_response_200>) {
                BOOST_CHECK_EQUAL(arg.body, "success");
            }
        },
                   sR);
        sR = sClient.get_permanent({}); // 2nd call
    }
}
BOOST_FIXTURE_TEST_CASE(discovery, WithServer)
{
    Discovery                sDiscovery;
    SD::NotifyWeight::Params sParams;
    sParams.location = "test-location";
    sDiscovery.with_discovery(m_Asio.service(), "127.0.0.1:3000", "test-service", sParams);
    sDiscovery.configure(m_Router);

    api::discovery_1_0::client sClient(m_HttpClient, m_Asio.service(), "test-service");
    sClient.__wait();

    {
        // after client.__wait, to ensure server have registered
        Etcd::Client::Params sParams;
        Etcd::Client         sClient(m_Asio.service(), sParams);
        auto                 sList = sClient.list("discovery/swagger");
        BOOST_REQUIRE_EQUAL(1, sList.size());
        BOOST_CHECK_EQUAL(sList[0].key, "discovery/swagger/test-service/discovery/1.0/127.0.0.1:3000");
        BOOST_CHECK_EQUAL(sList[0].value, R"({"location":"test-location","rps":0.001,"threads":1,"weight":10.0})");
    }

    auto sResponse = sClient.get_discovery({});
    BOOST_CHECK_EQUAL(sResponse.body, "success");

    // test prometheus exporter

    Prometheus::configure(m_Router);
    Prometheus::start(m_Group);

    auto sMD = std::make_shared<Swagger::MetricsDiscovery>(m_Asio.service(), SD::Balancer::Params{});
    sMD->start();
    sMD->configure(m_Router);
    Threads::sleep(0.1); // wait until MD make a step
    BOOST_CHECK_EQUAL(R"([{"labels":{"location":"test-location","service":"test-service"},"targets":["127.0.0.1:3000"]}])", sMD->to_string());

    // log metrics
    const auto sActual = Prometheus::Manager::instance().toPrometheus();
    for (auto x : sActual)
        BOOST_TEST_MESSAGE(x);

    // sleep(600);
}
BOOST_FIXTURE_TEST_CASE(breaker, WithServer)
{
    Discovery500             sDiscovery;
    SD::NotifyWeight::Params sParams;
    sParams.location = "test-location";
    sDiscovery.with_discovery(m_Asio.service(), "127.0.0.1:3000", "test-service", sParams);
    sDiscovery.configure(m_Router);

    api::discovery_1_0::client sClient(m_HttpClient, m_Asio.service(), "test-service");
    sClient.__wait();

    bool sBreaked = false;
    for (int i = 0; i < 20 and !sBreaked; i++) {
        try {
            auto sResponse = sClient.get_discovery({});
            BOOST_CHECK_EQUAL(sResponse.body, "success");
        } catch (const SD::Balancer::Error& e) {
            BOOST_TEST_MESSAGE("balancer error: " << e.what());
            sBreaked = true;
        } catch (const std::exception& e) {
            BOOST_TEST_MESSAGE("client error: " << e.what());
        }
        sleep(1);
    }
    BOOST_CHECK_EQUAL(sBreaked, true);

    // log metrics
    const auto sActual = Prometheus::Manager::instance().toPrometheus();
    for (auto x : sActual)
        BOOST_TEST_MESSAGE(x);
}
BOOST_AUTO_TEST_SUITE_END() // rpc

BOOST_AUTO_TEST_SUITE(http2)
BOOST_FIXTURE_TEST_CASE(simple, WithServerV2)
{
    Common sCommon;
    sCommon.configure(m_Router);

    api::common_1_0::client sClient(m_HttpClient, "127.0.0.1:3000");

    auto sResponse = sClient.get_enum({});

    const std::vector<std::string> sExpected = {{"one"}, {"two"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(),
                                  sExpected.end(),
                                  sResponse.body.begin(),
                                  sResponse.body.end());
}
BOOST_AUTO_TEST_SUITE_END()
