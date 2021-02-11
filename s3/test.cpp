#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "S3.hpp"

BOOST_AUTO_TEST_SUITE(S3)
BOOST_AUTO_TEST_CASE(simple)
{
    S3::Params sParams;
    S3::API    sAPI(sParams);
    auto       sResult = sAPI.PUT("some_file", "test data");

    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_TEST_MESSAGE("headers: ");
    for (auto& [sHeader, sValue] : sResult.headers)
        BOOST_TEST_MESSAGE(sHeader << " = " << sValue);
    BOOST_TEST_MESSAGE("body: " << sResult.body);

    sResult = sAPI.GET("some_file");
    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_CHECK_EQUAL(sResult.body, "test data");
}
BOOST_AUTO_TEST_SUITE_END()
