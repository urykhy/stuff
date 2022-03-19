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
    BOOST_CHECK_EQUAL(String::replace("foo", "foo", "long bar"), "long bar");
    BOOST_CHECK_EQUAL(String::replace_all("foo bar foo bar foo no more bar foo", "foo", "x"), "x bar x bar x no more bar x");
}
BOOST_AUTO_TEST_SUITE_END()
