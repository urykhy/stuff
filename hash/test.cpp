#define BOOST_TEST_MODULE Suites

#include <boost/test/unit_test.hpp>

#include "Rendezvous.hpp"
#include "Ring.hpp"

#include <format/List.hpp>
#include <unsorted/Random.hpp>

// clang-format off
#include <string.h>
#include "gperf.hpp"
// clang-format on

BOOST_AUTO_TEST_SUITE(rendezvous)
BOOST_AUTO_TEST_CASE(basic)
{
    const Hash::Rendezvous::ServerList sList = {
        {"test1"},
        {"test2"},
        {"test3:a"},
        {"test3:b"},
        {"test3:c"},
        {"test3:d"},
        {"test4:a"},
        {"test4:b"},
    };
    Hash::Rendezvous sHash(sList);

    const uint32_t                       COUNT = 1e6;
    std::map<std::string_view, uint32_t> sStat;
    for (uint32_t i = 0; i < COUNT; i++) {
        const std::string sKey = "test value " + std::to_string(i);
        sStat[sHash(sKey)]++;
    }
    for (auto& [sName, sCount] : sStat) {
        BOOST_TEST_MESSAGE(sName);
        BOOST_CHECK_CLOSE((double)COUNT / sList.size(), (double)sCount, 1);
    }

    BOOST_CHECK_EQUAL("test2", sHash(123));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ring)
BOOST_AUTO_TEST_CASE(basic)
{
    std::vector<std::string> sNames{"dc01my01:3306", "dc02my01:3306", "dc02my02:3306", "dc03my01:3306", "dc03my02:3306"};

    Hash::Ring sRing;
    sRing.insert(sNames[0], 0, 1, 10); // name, server-id, datacenter-id, weight
    sRing.insert(sNames[1], 1, 2, 5);
    sRing.insert(sNames[2], 2, 2, 15);
    sRing.insert(sNames[3], 3, 3, 20);
    sRing.insert(sNames[4], 4, 3, 20);
    sRing.prepare();
    // total weight: 70

    Util::seed();

    std::map<std::string, uint32_t> sCalls;
    std::map<std::string, uint32_t> sOne;
    const unsigned                  CALLS = 1e6;
    for (unsigned i = 0; i < CALLS; i++) {
        auto       sVal  = i % 10000;
        auto       sHash = XXH3_64bits_withSeed(&sVal, sizeof(sVal), 0);
        const auto sIDS  = sRing(sHash);
        for (auto sID : sIDS) {
            const std::string& sName = sNames[sID];
            if (Util::drand48() < 0.95) {
                sCalls[sName]++;
                if (sVal == 10)
                    sOne[sName]++;
                break;
            }
        };
    }
    BOOST_TEST_MESSAGE("total calls:");
    for (auto& x : sCalls)
        BOOST_TEST_MESSAGE(x.first << "\t" << x.second);
    BOOST_TEST_MESSAGE("one key:");
    for (auto& x : sOne)
        BOOST_TEST_MESSAGE(x.first << "\t" << x.second);
    BOOST_TEST_MESSAGE("servers for one key: " << [&]() -> std::string {
        const unsigned    sVal  = 10;
        auto              sHash = XXH3_64bits_withSeed(&sVal, sizeof(sVal), 0);
        std::stringstream sTmp;
        Format::List(sTmp, sRing(sHash), [&](auto sID) { return sNames[sID]; });
        return sTmp.str();
    }());

    BOOST_CHECK_CLOSE((double)CALLS / 70 * 5, sCalls["dc02my01:3306"], 10);
    BOOST_CHECK_CLOSE((double)CALLS / 70 * 20, sCalls["dc03my01:3306"], 10);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(perfect)
BOOST_AUTO_TEST_CASE(gperf)
{
    BOOST_CHECK_EQUAL(test_hash_gperf::in_word_set("enum", 4), nullptr);
    BOOST_CHECK_NE(test_hash_gperf::in_word_set("name", 4), nullptr);
}
BOOST_AUTO_TEST_SUITE_END()