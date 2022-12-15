#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#if __has_include(<tsl/hopscotch_map.h>)
#include <tsl/hopscotch_map.h>
#endif

#include <boost/mpl/list.hpp>

#include "LFU.hpp"
#include "LRU.hpp"
#include "S_LRU.hpp"

#define FILE_NO_ARCHIVE
#include <file/File.hpp>
#include <format/Float.hpp>
#include <parser/Atoi.hpp>
#include <time/Meter.hpp>

// clang-format off
using CacheTypes = boost::mpl::list<
    Cache::LRU<int, int>
  , Cache::S_LRU<int, int>
  , Cache::LFU<int, int>
  , Cache::BF_LFU<int, int>
#if __has_include(<tsl/hopscotch_map.h>)
  , Cache::BF_LFU<int, int, tsl::hopscotch_map>
#endif
    >;
// clang-format on

const std::vector<int> gKeys = []() {
    // s3.arc can be found in
    // https://github.com/dgraph-io/benchmarks/tree/master/cachebench/ristretto/trace
    const std::string sFilename = "/u03/crap/trace/s3.arc";
    const uint32_t    sItems    = 16407702;
    std::vector<int>  sData;
    sData.reserve(sItems);
    File::by_string(sFilename, [&](const std::string_view aStr) mutable {
        auto sSpace = aStr.find(' ');
        if (sSpace == std::string_view::npos)
            return;
        auto sVal = Parser::Atoi<uint64_t>(aStr.substr(0, sSpace));
        sData.push_back(sVal);
    });
    return sData;
}();

BOOST_AUTO_TEST_SUITE(Bench)
BOOST_AUTO_TEST_CASE_TEMPLATE(s3_arc, CacheType, CacheTypes)
{
    /*
        16M requests, 1.6M uniq keys

        LRU      : 12%
        S_LRU    : 24%
        LFU      : 29%
        BF_LFU   : 42%
    */
    const unsigned CACHE_SIZE = 400000;
    CacheType      sCache(CACHE_SIZE);
    unsigned       sHits = 0;
    Time::Meter    sMeter;
    for (auto sVal : gKeys) {
        if (sCache.Get(sVal))
            sHits++;
        else
            sCache.Put(sVal, 0);
    };
    double sELA = sMeter.get().to_double();

    BOOST_TEST_MESSAGE("Got " << sHits << " hits from " << gKeys.size() << " requests, hit rate: " << sHits * 100 / double(gKeys.size()) << "%, RPS: " << Format::for_human(gKeys.size() / sELA));
}
BOOST_AUTO_TEST_SUITE_END()
