#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <boost/mpl/list.hpp>

#include "Expiration.hpp"
#include "LFU.hpp"
#include "LRU.hpp"
#include "S_LRU.hpp"

#define FILE_NO_ARCHIVE
#include <file/File.hpp>
#include <parser/Atoi.hpp>
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
    cache.debug([](auto& x) { if (x.key == 10 ) BOOST_CHECK_EQUAL(x.freq, 41); });

    for (int i = 20; i < 30; i++)
        cache.Put(i, i);

    cache.debug([](auto& x) { if (x.key == 11 ) BOOST_CHECK_EQUAL(x.freq, 22); });

    cache.debug([](auto& x) { BOOST_TEST_MESSAGE(x.key << ' ' << x.freq); });
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
BOOST_AUTO_TEST_SUITE(Bench)
using CacheTypes = boost::mpl::list<Cache::LRU<int, int>, Cache::S_LRU<int, int>, Cache::LFU<int, int>,  Cache::BF_LFU<int, int>>;
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
BOOST_AUTO_TEST_CASE(s3_arc, *boost::unit_test::disabled())
{
    /*
        s3.arc can be found in
        https://github.com/dgraph-io/benchmarks/tree/master/cachebench/ristretto/trace

        16M requests, 1.6M uniq keys

        LRU      : 12%
        S_LRU    : 24%
        LFU      : 29%
        BF_LFU   : 42%
    */
    const unsigned    CACHE_SIZE = 400000;
    const std::string sFilename  = "/u03/crap/trace/s3.arc";

    unsigned sHits = 0;
    unsigned sRows = 0;

    Cache::BF_LFU<int, int> sCache(CACHE_SIZE);
    File::by_string(sFilename, [&](const std::string_view aStr) mutable {
        auto sSpace = aStr.find(' ');
        if (sSpace == std::string_view::npos)
            return;
        auto sVal = Parser::Atoi<uint64_t>(aStr.substr(0, sSpace));
        sRows++;

        if (sCache.Get(sVal))
            sHits++;
        else
            sCache.Put(sVal, 0);
    });

    BOOST_TEST_MESSAGE("Got " << sHits << " hits from " << sRows << " requests, hit rate: " << sHits * 100 / double(sRows) << '%');
}
BOOST_AUTO_TEST_SUITE_END()