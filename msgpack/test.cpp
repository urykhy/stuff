#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <MsgPack.hpp>
#include <iostream>
#include <cassert>

const std::list<uint32_t> sTestData{0, 1, 14, 15, 16, 30, 31, 32, 127, 128, 129,
                                    UINT8_MAX-1, UINT8_MAX, UINT8_MAX+1,
                                    UINT16_MAX-1, UINT16_MAX, UINT16_MAX+1,
                                    UINT32_MAX-1, UINT32_MAX};

BOOST_AUTO_TEST_SUITE(MsgPack)
BOOST_AUTO_TEST_CASE(simple)
{
    MsgPack::binary sbuf;

    for (uint64_t i : sTestData)
    {
        MsgPack::omemstream os(sbuf);
        MsgPack::write_uint(os, i);

        uint64_t x = 0;
        MsgPack::imemstream is(sbuf);
        MsgPack::read_uint(is, x);
        BOOST_CHECK_EQUAL(i, x);

        sbuf.clear();
    }
}
BOOST_AUTO_TEST_CASE(map)
{
    MsgPack::binary sbuf;

    for (uint64_t i : sTestData)
    {
        MsgPack::omemstream os(sbuf);
        MsgPack::write_map_size(os, i);

        MsgPack::imemstream is(sbuf);
        uint64_t x = MsgPack::read_map_size(is);

        BOOST_CHECK_EQUAL(i, x);
        sbuf.clear();
    }
}
BOOST_AUTO_TEST_CASE(array)
{
    MsgPack::binary sbuf;

    for (uint64_t i : sTestData)
    {
        MsgPack::omemstream os(sbuf);
        MsgPack::write_array_size(os, i);

        MsgPack::imemstream is(sbuf);
        uint64_t x = MsgPack::read_array_size(is);

        BOOST_CHECK_EQUAL(i, x);
        sbuf.clear();
    }
}
BOOST_AUTO_TEST_SUITE_END()