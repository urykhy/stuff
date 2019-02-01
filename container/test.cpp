#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <map>
#include <Merge.hpp>

// g++ test.cpp -I. -lboost_unit_test_framework -lboost_system

BOOST_AUTO_TEST_SUITE(Container)
BOOST_AUTO_TEST_CASE(Merge)
{
    std::map<int, double> mapA = { {0, 1.0}, {1, 2.0} };
    std::map<int, double> mapB = { {1, 1.5}, {2, 2.5} };
    std::map<int, double> mapR = { {0, 1.0}, {1, 3.5}, {2, 2.5} };

    merge(mapA, mapB, [](double lhs, double rhs){ return lhs + rhs; });
    BOOST_CHECK(mapA == mapR);
}
BOOST_AUTO_TEST_SUITE_END()
