#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <Expiration.hpp>
#include <LRU.hpp>
#include <S_LRU.hpp>

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
BOOST_AUTO_TEST_CASE(s_lru)
{
    Cache::S_LRU<int, int> cache(10);

    for (int i = 0; i < 20; i++)
        cache.Put(i, i);
    cache.debug([i = 0](auto& x) mutable { BOOST_CHECK_EQUAL(15 + i, x->key); i++; });

    // 15 moved to protected segment
    cache.Get(15);
    cache.debug([](auto& x) { if (x->key == 15) BOOST_CHECK_EQUAL(x->prot, true); });

    // add 5 new numbers and hit on em to move in protected list
    // 15 must be moved back to normal
    for (int i = 0; i < 5; i++) {
        cache.Put(20 + i, 20 + i);
        cache.Get(20 + i);
    }
    cache.debug([](auto& x) {if (x->key == 15)BOOST_CHECK_EQUAL(x->prot, false); });

    // add new 5 elements, 15 must be dropped
    for (int i = 0; i < 5; i++) {
        cache.Put(30 + i, 30 + i);
    }
    BOOST_CHECK_EQUAL(cache.Get(15), nullptr);

    cache.debug([](auto& x) { BOOST_TEST_MESSAGE("element: " << x->key << ' ' << (x->prot ? "prot" : "norm")); });
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
            BOOST_CHECK(e.Get(i) == nullptr);
        else
            BOOST_CHECK(e.Get(i) != nullptr);
    }
    sleep(2);
    BOOST_CHECK(e.Get(9) == nullptr);
}
BOOST_AUTO_TEST_SUITE_END()