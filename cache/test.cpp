#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <boost/mpl/list.hpp>

#include "Expiration.hpp"
#include "LFU.hpp"
#include "LRU.hpp"
#include "Redis.hpp"
#include "S_LRU.hpp"

#define FILE_NO_ARCHIVE
#include <file/File.hpp>
#include <format/Float.hpp>
#include <parser/Atoi.hpp>
#include <time/Meter.hpp>
#include <unsorted/Random.hpp>

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
BOOST_AUTO_TEST_CASE(lfu)
{
    Cache::LFU<int, int> cache(10);

    for (int i = 0; i < 20; i++)
        cache.Put(i, i);

    cache.Get(10);
    cache.Get(10);
    cache.Get(11);
    cache.Debug([](auto& x) { if (x.key == 10 ) BOOST_CHECK_EQUAL(x.bucket, 41); });

    for (int i = 20; i < 30; i++)
        cache.Put(i, i);

    cache.Debug([](auto& x) { if (x.key == 11 ) BOOST_CHECK_EQUAL(x.bucket, 22); });

    cache.Debug([](auto& x) { BOOST_TEST_MESSAGE(x.key << ' ' << x.bucket); });

    BOOST_CHECK_NE(cache.Get(11), nullptr);
    cache.Remove(11);
    BOOST_CHECK_EQUAL(cache.Get(11), nullptr);
}
BOOST_AUTO_TEST_CASE(expiration)
{
    uint64_t                                               sNow = 123;
    Cache::ExpirationAdapter<int, std::string, Cache::LRU> sCache(5 /* max size */, 2 /* deadline */);
    BOOST_CHECK_EQUAL(sCache.Size(), 0);
    sCache.Put(1, "test", sNow);
    BOOST_CHECK_EQUAL(sCache.Size(), 1);
    sCache.Put(2, "bar", sNow);
    BOOST_CHECK_EQUAL(sCache.Size(), 2);
    BOOST_CHECK(sCache.Get(1, sNow) != nullptr);

    for (int i = 0; i < 10; i++) {
        sCache.Put(i, std::to_string(i), sNow);
    }
    for (int i = 0; i < 10; i++) {
        if (i < 5)
            BOOST_CHECK(sCache.Get(i, sNow) == nullptr);
        else
            BOOST_CHECK(sCache.Get(i, sNow) != nullptr);
    }
    BOOST_CHECK_EQUAL(sCache.Size(), 5);
    sNow += 10;
    BOOST_CHECK(sCache.Get(9, sNow) == nullptr);
    BOOST_CHECK_EQUAL(sCache.Size(), 4);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Bench)
using CacheTypes = boost::mpl::list<Cache::LRU<int, int>, Cache::S_LRU<int, int>, Cache::LFU<int, int>, Cache::BF_LFU<int, int>>;
BOOST_AUTO_TEST_CASE_TEMPLATE(zipf, T, CacheTypes)
{
    Util::seed();

    const unsigned KEY_COUNT  = 10000;
    const unsigned CACHE_SIZE = KEY_COUNT * 0.1; // 10%
    const unsigned CALLS      = KEY_COUNT * 10;  // x10
    const double   ALPHA      = 0.8;

    Util::Zipf sDist(KEY_COUNT, ALPHA);
    T          sCache(CACHE_SIZE);
    unsigned   sHits = 0;
    for (unsigned i = 0; i < CALLS; i++) {
        auto sVal = sDist();
        if (sCache.Get(sVal))
            sHits++;
        else
            sCache.Put(sVal, 0);
    }
    BOOST_TEST_MESSAGE("Got " << sHits << " hits from " << CALLS << " requests, hit rate: " << sHits * 100 / double(CALLS) << '%');
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Redis)
BOOST_AUTO_TEST_CASE(basic)
{
    Cache::Redis::Config  sConfig;
    Cache::Redis::Manager sRedis(sConfig);
    sRedis.set("basic", "value");
    auto sResult = sRedis.get("basic");
    BOOST_CHECK_EQUAL(*sResult, "value");
}
BOOST_AUTO_TEST_SUITE_END()
