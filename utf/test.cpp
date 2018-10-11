#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <UTF8.hpp>

// g++ test.cpp -I. -lboost_system -lboost_unit_test_framework -ggdb -std=c++14 -O3
// ./a.out -l all -t UTF8/simple

BOOST_AUTO_TEST_SUITE(UTF8)
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
