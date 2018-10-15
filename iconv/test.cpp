#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <Iconv.hpp>

// g++ test.cpp -I. -lboost_system -lboost_unit_test_framework -ggdb -std=c++14 -O3
// ./a.out -l all

BOOST_AUTO_TEST_SUITE(Iconv)
BOOST_AUTO_TEST_CASE(to_cp1251)
{
    Iconv::Convert sConvert("UTF-8", "CP1251");
    BOOST_CHECK_THROW(sConvert("\xFF"), std::runtime_error);
    BOOST_CHECK_EQUAL(sConvert("тест"), "\xf2\xe5\xf1\xf2");
}
BOOST_AUTO_TEST_CASE(from_cp1251)
{
    Iconv::Convert sConvert("CP1251", "UTF-8");
    BOOST_CHECK_EQUAL(sConvert("\xf2\xe5\xf1\xf2"), "тест");
}
BOOST_AUTO_TEST_SUITE_END()
