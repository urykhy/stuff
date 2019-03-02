#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <Gzip.hpp>
#include <__static.cpp>

#define DECODER(NAME)                                           \
{                                                               \
    BOOST_TEST_MESSAGE("testing " << #NAME);                    \
    boost::string_ref sData((const char*)test_##NAME, test_##NAME##_len); \
    auto sUnpacker = Gzip::make_unpacker(#NAME);                \
    std::string sResult;                                        \
    auto sChunk = (*sUnpacker)(sData);                          \
    sResult.append(sChunk.data(), sChunk.size());               \
    sChunk = (*sUnpacker)(""); /* EOF marker */                 \
    sResult.append(sChunk.data(), sChunk.size());               \
    BOOST_CHECK_EQUAL(sResult, "test stream\n");                \
}

BOOST_AUTO_TEST_SUITE(GZIP)
BOOST_AUTO_TEST_CASE(simple)
{
    DECODER(gz)
    DECODER(bz2)
    DECODER(xz)
    DECODER(lz4)
    DECODER(zst)
}
BOOST_AUTO_TEST_SUITE_END()
