#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Rendezvous.hpp"

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

    const uint32_t COUNT = 1e6;
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
