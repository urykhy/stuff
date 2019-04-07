#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <map>
#include <Merge.hpp>
#include <ListArray.hpp>
#include <IoC.hpp>

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
BOOST_AUTO_TEST_CASE(listArray)
{
	Container::ListArray<size_t> a(10);
	BOOST_CHECK(a.size() == 0);
	const size_t max = 25;
	for (size_t i = 0; i < max; i++) a.push_back(i);
	BOOST_CHECK(a.begin() == a.begin());
	BOOST_CHECK(a.end() == a.end());
	BOOST_CHECK(a.begin() != a.end());
	BOOST_CHECK(a.size() == max);
	size_t c = 0;
	for (const auto& x : a) {
		BOOST_CHECK(c++ == x);
	}
}
BOOST_AUTO_TEST_CASE(ioc)
{
    Container::IoC r;
    int i = 10; r.Put(i);
    double j = 20; r.Put(j);

    BOOST_CHECK(10 == r.Get<int>());
    BOOST_CHECK(abs(20 - r.Get<double>()) < 0.001 );
}
BOOST_AUTO_TEST_SUITE_END()