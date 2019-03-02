#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <Gzip.hpp>
#include <__static.cpp>

#define DECODER(NAME)                                                       \
{                                                                           \
    BOOST_TEST_MESSAGE("testing " << #NAME);                                \
    boost::string_ref sData((const char*)test_##NAME, test_##NAME##_len);   \
    BOOST_CHECK_EQUAL(Gzip::decode_buffer(Gzip::get_format(#NAME), sData),  \
                      "test stream\n");                                     \
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
BOOST_AUTO_TEST_CASE(concat)
{
    // 2 gzip archives in one file
    boost::string_ref sData((const char*)concat_gz, concat_gz_len);
    std::string sResult=Gzip::decode_buffer(Gzip::FORMAT::GZIP, sData);
    BOOST_CHECK_EQUAL(sResult, "test stream\nother line\n");
}
BOOST_AUTO_TEST_SUITE_END()
