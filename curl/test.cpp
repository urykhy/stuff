#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <Curl.hpp>

// g++ test.cpp -I. -lboost_unit_test_framework -lcurl

BOOST_AUTO_TEST_SUITE(Curl)
BOOST_AUTO_TEST_CASE(Basic)
{
    Curl::Client::Params sParams;
    sParams.headers.push_back({"XFF","value"});
    sParams.cookie   = "name1=content1;";
    sParams.username = "name";
    sParams.password = "secret";

    Curl::Client sClient(sParams);
    auto sResult = sClient.GET("http://127.0.0.1:8080/hello");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "Hello World!");

    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/useragent").second, "Curl/Client");
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/header").second, "value");
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/cookie").second, "content1");
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/auth").first, 200);

    sParams.username = "othername";
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/auth").first, 401);
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/nx_location").first, 404);

    BOOST_CHECK_THROW(sClient.GET("http://127.0.0.1:8080/slow"), Curl::Client::Error);
}
BOOST_AUTO_TEST_CASE(Methods)
{
    Curl::Client::Params sParams;
    Curl::Client sClient(sParams);

    auto sResult = sClient.POST("http://127.0.0.1:8080/post_handler", "W=123");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "post: 123");

    sResult = sClient.PUT("http://127.0.0.1:8080/method_handler", "some data");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "PUT: some data");

    sResult = sClient.DELETE("http://127.0.0.1:8080/method_handler");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "OK");

}
BOOST_AUTO_TEST_CASE(Stream)
{
    Curl::Client::Params sParams;
    Curl::Client sClient(sParams);
    std::string sResult;
    int sCode = sClient.GET("http://127.0.0.1:8080/hello", [&sResult](void* aPtr, size_t aSize) -> size_t {
        sResult.append((char*)aPtr, aSize);
        return aSize;
    });
    BOOST_CHECK_EQUAL(sCode, 200);
    BOOST_CHECK_EQUAL(sResult, "Hello World!");
}
BOOST_AUTO_TEST_SUITE_END()
