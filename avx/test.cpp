#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Search.hpp"

BOOST_AUTO_TEST_SUITE(Binsearch)
BOOST_AUTO_TEST_CASE(avx1)
{
    constexpr unsigned            COUNT = 8;
    Util::alignedVector<uint32_t> sData;
    for (unsigned i = 0; i < COUNT; i++)
        sData.push_back(i);

    const WideInt32* sInput = reinterpret_cast<const WideInt32*>(&sData[0]);
    for (unsigned i = 0; i <= COUNT; i++)
        BOOST_REQUIRE_EQUAL(i, Util::avxLowerBound1(*sInput, i));
}
BOOST_AUTO_TEST_CASE(avx4)
{
    constexpr unsigned            COUNT = 8 * 4;
    Util::alignedVector<uint32_t> sData;
    for (unsigned i = 0; i < COUNT; i++)
        sData.push_back(i);

    const WideInt32* sInput = reinterpret_cast<const WideInt32*>(&sData[0]);
    for (unsigned i = 0; i <= COUNT; i++)
        BOOST_REQUIRE_EQUAL(i, Util::avxLowerBound4(*sInput, i));
}
BOOST_AUTO_TEST_CASE(avxIndex)
{
    constexpr unsigned BLOCK_COUNT = 8;
    const auto         sMake       = [](unsigned aCount) {
        Util::alignedVector<uint32_t> sData;
        const unsigned                COUNT = aCount * 8; // 8*32 = 256 = avx register size
        sData.reserve(COUNT);
        for (unsigned i = 0; i < COUNT; i++)
            sData.push_back(i);
        return sData;
    };
    const auto     sVec = sMake(BLOCK_COUNT);
    Util::avxIndex sAvxIndex(sVec);

    for (unsigned i = 0; i < 8 * BLOCK_COUNT; i++)
        BOOST_REQUIRE_EQUAL(i, sAvxIndex.lower_bound(i));
}
BOOST_AUTO_TEST_CASE(avxIndexMass)
{
    constexpr unsigned BLOCK_COUNT = 262144;

    const auto sMake = [](unsigned aCount) {
        Util::alignedVector<uint32_t> sData;
        const unsigned                COUNT = aCount * 8; // 8*32 = 256 = avx register size
        sData.reserve(COUNT);
        for (unsigned i = 0; i < COUNT; i++)
            sData.push_back(i);
        return sData;
    };
    const auto     sVec = sMake(BLOCK_COUNT);
    Util::avxIndex sAvxIndex(sVec);

    for (unsigned i = 0; i < 8 * BLOCK_COUNT; i++)
        BOOST_REQUIRE_EQUAL(i, sAvxIndex.lower_bound(i));
}
BOOST_AUTO_TEST_SUITE_END()