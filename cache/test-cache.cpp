/*
 *
 * g++ test-cache.cpp -I. -lboost_unit_test_framework
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <LRU.hpp>
#include <LFU.hpp>
#include <Expiration.hpp>
#include <IoC.hpp>
#include <ListArray.hpp>

BOOST_AUTO_TEST_SUITE(Cache)
BOOST_AUTO_TEST_CASE(lru)
{
    Cache::LRU<int, int> cache(10);
    BOOST_CHECK(cache.Get(1) == nullptr);

    // drop element from cache
	for (int i = 0; i < 20; i++) {
		cache.Put(i, i);
	}
	for (int i = 0; i < 20; i++) {
		if (i < 10) {
			BOOST_CHECK(cache.Get(i) == nullptr);
		} else {
			BOOST_CHECK(*cache.Get(i) == i);
		}
	}

    // update
	for (int i = 0; i < 20; i++) {
		cache.Put(i, i % 10);
	}
	for (int i = 0; i < 20; i++) {
		if (i < 10) {
			BOOST_CHECK(cache.Get(i) == nullptr);
		} else {
			BOOST_CHECK(*cache.Get(i) == i % 10);
		}
	}
}
BOOST_AUTO_TEST_CASE(lfu)
{
    Cache::LFU<int, int> cache(10);
    BOOST_CHECK(cache.Get(1) == nullptr);

    // drop element from cache
	for (int i = 0; i < 20; i++) {
		cache.Put(i, i);
	}
	for (int i = 0; i < 20; i++) {
		if (i < 10) {
			BOOST_CHECK(cache.Get(i) == nullptr);
		} else {
			BOOST_CHECK(*cache.Get(i) == i);
		}
	}

    // in cache elements [10..20)
    cache.Get(19);
    cache.Get(19);
    cache.Get(18);
    cache.Print();
}
BOOST_AUTO_TEST_CASE(expiration)
{
    Cache::Expiration<int, std::string> e(5, 2);
    BOOST_CHECK(e.size() == 0);
    e.Put(1, "test");
    BOOST_CHECK(e.size() == 1);
    e.Put(2, "bar");
    BOOST_CHECK(e.size() == 2);
    BOOST_CHECK(e.Get(1) != nullptr);

    for (int i = 0; i < 10; i++) {
        e.Put(i, std::to_string(i));
    }
    for (int i = 0; i < 10; i++) {
        if (i < 5)
            BOOST_CHECK(e.Get(i) == nullptr );
        else
            BOOST_CHECK(e.Get(i) != nullptr );
    }
    sleep(2);
    BOOST_CHECK(e.Get(9) == nullptr );
}
BOOST_AUTO_TEST_CASE(ioc)
{
    Cache::IoC r;
    int i = 10; r.Put(i);
    double j = 20; r.Put(j);

    BOOST_CHECK(10 == r.Get<int>());
    BOOST_CHECK(abs(20 - r.Get<double>()) < 0.001 );
}
BOOST_AUTO_TEST_CASE(listArray)
{
	Cache::ListArray<size_t> a(10);
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
BOOST_AUTO_TEST_SUITE_END()
