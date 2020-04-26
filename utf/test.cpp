#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <UTF8.hpp>

// g++ test.cpp -I. -lboost_system -lboost_unit_test_framework -ggdb -std=c++14 -O3
// ./a.out -l all -t UTF8

BOOST_AUTO_TEST_SUITE(UTF8)
BOOST_AUTO_TEST_CASE(sym)
{
    BOOST_CHECK_EQUAL(0x24, UTF8::Validate("\x24"));
    BOOST_CHECK_EQUAL(0xA2, UTF8::Validate("\xc2\xa2"));
    BOOST_CHECK_EQUAL(0x20AC, UTF8::Validate("\xe2\x82\xac"));
    BOOST_CHECK_EQUAL(0x817BC, UTF8::Validate("\xf2\x81\x9e\xbc"));

    BOOST_CHECK_THROW(UTF8::Validate("\xed\xb2\x80"), UTF8::BadCharacter);  // U+DC80
    BOOST_CHECK_THROW(UTF8::Validate("\xed\xb3\xbf"), UTF8::BadCharacter);  // U+DCFF
}
BOOST_AUTO_TEST_CASE(simple)
{
    // null character, long encoding
    BOOST_CHECK_THROW(UTF8::Validate("\xC0\x80"), UTF8::BadCharacter);
    BOOST_CHECK_THROW(UTF8::Validate("\xE0\x80\x80"), UTF8::BadCharacter);
    BOOST_CHECK_THROW(UTF8::Validate("\xF0\x80\x80\x80"), UTF8::BadCharacter);

    // extra continuation
    BOOST_CHECK_THROW(UTF8::Validate("\xC0\x80\x80"), UTF8::BadCharacter);
    // not enough continuation
    BOOST_CHECK_THROW(UTF8::Validate("\xF0\x80\x80"), UTF8::BadCharacter);
    // bad continuation
    BOOST_CHECK_THROW(UTF8::Validate("\xF0\xFF"), UTF8::BadCharacter);
    // UTF-16 Surrogates
    BOOST_CHECK_THROW(UTF8::Validate("\xED\xA0\x80"), UTF8::BadCharacter);

    // good string
    BOOST_CHECK_NO_THROW(UTF8::Validate("'Строка'"));
}
BOOST_AUTO_TEST_SUITE_END()
