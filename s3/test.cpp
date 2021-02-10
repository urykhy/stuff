#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "S3.hpp"

BOOST_AUTO_TEST_SUITE(S3)
BOOST_AUTO_TEST_CASE(simple)
{
    S3::Params sParams;
    auto       sResult = S3::PUT(sParams, "some_file", "test data");

    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_TEST_MESSAGE("data: " << sResult.second);
}
BOOST_AUTO_TEST_SUITE_END()
