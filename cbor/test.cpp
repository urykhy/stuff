#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <container/Stream.hpp>
#include <format/Hex.hpp>

#include "cbor-custom.hpp"
#include "cbor-proxy.hpp"
#include "cbor-tuple.hpp"
#include "cbor-optional.hpp"
#include "decoder.hpp"
#include "encoder.hpp"
#include "tutorial.hpp"


BOOST_AUTO_TEST_SUITE(Cbor)
BOOST_AUTO_TEST_CASE(list)
{
    cbor::omemstream out;
    cbor::write(out, 1, 5.0, std::string("string"), true);
    BOOST_TEST_MESSAGE(Format::to_hex(out.str()));

    cbor::imemstream in(out.str());
    int              a;
    double           b;
    std::string      c;
    bool             d;
    cbor::read(in, a, b, c, d);

    BOOST_CHECK_EQUAL(a, 1);
    BOOST_CHECK_EQUAL(b, 5.0);
    BOOST_CHECK_EQUAL(c, std::string("string"));
    BOOST_CHECK_EQUAL(d, true);

    // parsing as tuple
    using T = std::tuple<int, double, std::string, bool>;
    cbor::imemstream in1(out.str());
    T                tuple;
    cbor::read(in1, tuple);
    BOOST_CHECK(tuple == T(1, 5.0, std::string_view("string"), true));
}
BOOST_AUTO_TEST_CASE(tag)
{
    cbor::omemstream out;
    uint64_t         expected = time(0);
    cbor::write_tag(out, 1);
    cbor::write(out, expected);

    {
        cbor::imemstream in(out.str());
        uint64_t         actual = 0;
        cbor::read(in, actual); // skip tag
        BOOST_CHECK_EQUAL(actual, expected);
    }
    {
        cbor::imemstream in(out.str());
        uint64_t         actual = 0;
        uint64_t         tag    = 0;
        cbor::read_tag(in, tag); // collect tag
        cbor::read(in, actual);
        BOOST_CHECK_EQUAL(actual, expected);
        BOOST_CHECK_EQUAL(tag, 1);
    }
}
BOOST_AUTO_TEST_CASE(container)
{
    using List = std::list<std::string>;
    using Map  = std::map<int, List>;

    Map              data = {{1, {{"one"}, {"two"}, {"three"}}}, {2, {{"abc"}, {"cde"}}}};
    cbor::omemstream out;
    cbor::write(out, data);

    Map              actual;
    cbor::imemstream in(out.str());
    cbor::read(in, actual);
    BOOST_CHECK(data == actual);
}
BOOST_AUTO_TEST_CASE(proxy)
{
    auto sCall = cbor::make_proxy([](int a, std::string b, double c) -> float {
        BOOST_TEST_MESSAGE("args " << a << ", " << b << ", " << c);
        float rc = a + b.size() * c;
        BOOST_TEST_MESSAGE("return " << rc);
        return rc;
    });

    cbor::omemstream out;
    //cbor::write(out, 10, std::string_view("some"), 0.1);
    cbor::write(out, std::make_tuple(10, std::string_view("some"), 0.1));

    cbor::imemstream input(out.str());
    cbor::omemstream result;
    sCall->run(input, result);

    cbor::imemstream in(result.str());
    float            result_value = 0;
    cbor::read(in, result_value);
    BOOST_CHECK_CLOSE(result_value, 10.4, 0.1);
}
BOOST_AUTO_TEST_CASE(generated)
{
    cbor::binary tmp;
    {
        test::Vary sTmp;
        sTmp.name = "test";
        sTmp.age  = 12;
        sTmp.phone.emplace();
        sTmp.phone->insert(std::make_pair(1, "some number"));
        sTmp.city.emplace();
        sTmp.city->push_back("some city");

        cbor::omemstream out;
        cbor::write(out, sTmp);
        tmp = out.str();
        BOOST_TEST_MESSAGE(Format::to_hex(tmp));
    }
    {
        cbor::imemstream in(tmp);
        test::Vary       sTmp;
        cbor::read(in, sTmp);

        BOOST_CHECK_EQUAL(*sTmp.name, "test");
        BOOST_CHECK_EQUAL(*sTmp.age, 12);
        auto sIt = sTmp.phone->find(1);
        BOOST_REQUIRE(sIt != sTmp.phone->end());
        BOOST_CHECK_EQUAL(sIt->second, "some number");
        auto sXt = sTmp.city->begin();
        BOOST_REQUIRE(sXt != sTmp.city->end());
        BOOST_CHECK_EQUAL(*sXt, "some city");
    }
}
BOOST_AUTO_TEST_CASE(substring)
{
    cbor::omemstream out;
    cbor::write(out, "test string");

    Container::imemstream in(out.str());
    std::string_view      s;
    cbor::read(in, s);
    BOOST_CHECK_EQUAL(s, "test string");
}
BOOST_AUTO_TEST_CASE(optional)
{
    std::optional<std::string> sVal;
    cbor::omemstream out;
    cbor::write(out, std::string("test"));
    cbor::write(out, sVal);

    Container::imemstream in(out.str());
    cbor::read(in, sVal);
    BOOST_CHECK_EQUAL(true, sVal.has_value());
    BOOST_CHECK_EQUAL("test", sVal.value());
    cbor::read(in, sVal);
    BOOST_CHECK_EQUAL(false, sVal.has_value());
}
BOOST_AUTO_TEST_SUITE_END()