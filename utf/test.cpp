#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <UTF8.hpp>

BOOST_AUTO_TEST_SUITE(UTF8)
BOOST_AUTO_TEST_CASE(sym)
{
    BOOST_CHECK_EQUAL(0x24, UTF8::Decode("\x24"));
    BOOST_CHECK_EQUAL(0xA2, UTF8::Decode("\xc2\xa2"));
    BOOST_CHECK_EQUAL(0x20AC, UTF8::Decode("\xe2\x82\xac"));
    BOOST_CHECK_EQUAL(0x817BC, UTF8::Decode("\xf2\x81\x9e\xbc"));

    BOOST_CHECK_THROW(UTF8::Decode("\xed\xb2\x80"), UTF8::BadCharacter); // U+DC80
    BOOST_CHECK_THROW(UTF8::Decode("\xed\xb3\xbf"), UTF8::BadCharacter); // U+DCFF
    BOOST_CHECK_THROW(UTF8::Decode("\x24\x24"), std::invalid_argument); // not a signle character
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

    // string length in code points
    BOOST_CHECK_EQUAL(UTF8::Length("test строка"), 11);

    // allow null characters
    std::string sTmp("tttt\x00\x00\x00rrrr", 11);
    BOOST_CHECK_EQUAL(UTF8::Length(sTmp), 11);

    // plane and range
    BOOST_CHECK_EQUAL(UTF8::IsBMP("тест string"), true);
    BOOST_CHECK_EQUAL(UTF8::IsBMP("\xf0\x90\x85\x83"), false); // GREEK ACROPHONIC ATTIC FIVE

    BOOST_CHECK_EQUAL(UTF8::InRange("test", UTF8::BasicLatin), true);
    BOOST_CHECK_EQUAL(UTF8::InRange("тест", UTF8::BasicLatin), false);
    BOOST_CHECK_EQUAL(UTF8::InRange("тест string", UTF8::BasicCyr, UTF8::BasicLatin), true);
}
BOOST_AUTO_TEST_CASE(iterate)
{
    std::vector<uint32_t> sExpected{0x0442, 0x0435, 0x0441, 0x0442, 0x0020, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067};
    std::vector<uint32_t> sResult;
    UTF8::CharDecoder     sDecoder("тест string");
    for (auto x : sDecoder)
        sResult.push_back(x);
    BOOST_CHECK_EQUAL_COLLECTIONS(sResult.begin(), sResult.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_SUITE_END()
