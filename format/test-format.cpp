#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <sstream>
#include <list>
#include <List.hpp>

#if 0
g++ test-format.cpp -std=c++14 -I . -lboost_unit_test_framework
#endif

BOOST_AUTO_TEST_SUITE(Format)
BOOST_AUTO_TEST_CASE(list)
{
    std::stringstream a;
    std::list<int> list{1,2,3};
    Format::List(a, list);
    BOOST_CHECK_EQUAL(a.str(), "1, 2, 3");

    a.str("");
    Format::List(a, list, [](int x ) -> std::string { return std::to_string(x*x); });
    BOOST_CHECK_EQUAL(a.str(), "1, 4, 9");
}
BOOST_AUTO_TEST_SUITE_END()
