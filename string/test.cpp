/*
 *
 * g++ test.cpp -I. -lboost_unit_test_framework -lboost_system -lboost_filesystem -DBOOST_ALL_DYN_LINK
 */

#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <String.hpp>

BOOST_AUTO_TEST_SUITE(String)
BOOST_AUTO_TEST_CASE(utils)
{
    std::string sTmp("  a b     \r\n\t");
    String::trim(sTmp);
    BOOST_CHECK_EQUAL(sTmp, "a b");
    BOOST_CHECK(String::starts_with(sTmp, "a"));
    BOOST_CHECK(String::ends_with(sTmp, " b"));
}
BOOST_AUTO_TEST_SUITE_END()
