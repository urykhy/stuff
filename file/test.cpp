/*
 *
 * g++ test.cpp -I. -lboost_unit_test_framework
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <File.hpp>

BOOST_AUTO_TEST_SUITE(File)
BOOST_AUTO_TEST_CASE(read)
{
    BOOST_CHECK_EQUAL(File::to_string("__test_data1"), "123\nasd\n");

    File::by_string("__test_data1", [i = 0](const std::string& a) mutable {
        switch(i)
        {
        case 0: BOOST_CHECK_EQUAL(a, "123"); break;
        case 1: BOOST_CHECK_EQUAL(a, "asd"); break;
        default:
                BOOST_CHECK(false);
        }
        i++;
    });
}
BOOST_AUTO_TEST_SUITE_END()
