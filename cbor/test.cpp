#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <parser/Hex.hpp>

#include <encoder.hpp>
#include <decoder.hpp>
#include <cbor-tuple.hpp>
#include <cbor-proxy.hpp>

#include "tutorial.hpp"

BOOST_AUTO_TEST_SUITE(Cbor)
BOOST_AUTO_TEST_CASE(list)
{
    cbor::binary buffer;
    cbor::omemstream out(buffer);
    cbor::write(out, 1, 5.0, boost::string_ref("string"), true);
    BOOST_TEST_MESSAGE(Parser::to_hex(std::string(buffer.begin(), buffer.end())));

    cbor::imemstream in(buffer);
    int a;
    double b;
    boost::string_ref c;
    bool d;
    cbor::read(in, a, b, c, d);

    BOOST_CHECK_EQUAL(a, 1);
    BOOST_CHECK_EQUAL(b, 5.0);
    BOOST_CHECK_EQUAL(c, boost::string_ref("string"));
    BOOST_CHECK_EQUAL(d, true);

    // parsing as tuple as well
    using T = std::tuple<int, double, boost::string_ref, bool>;
    cbor::imemstream in1(buffer);
    T tuple;
    cbor::read(in1, tuple);
    BOOST_CHECK(tuple == T(1, 5.0, boost::string_ref("string"), true));
}
BOOST_AUTO_TEST_CASE(tag)
{
    cbor::binary buffer;
    cbor::omemstream out(buffer);
    uint64_t expected = time(0);
    cbor::write_tag(out, 1);
    cbor::write(out, expected);

    {
        cbor::imemstream in(buffer);
        uint64_t actual = 0;
        cbor::read(in, actual); // skip tag
        BOOST_CHECK_EQUAL(actual, expected);
    }
    {
        cbor::imemstream in(buffer);
        uint64_t actual = 0;
        uint64_t tag = 0;
        BOOST_CHECK(cbor::read_tag(in, tag));
        BOOST_CHECK_EQUAL(tag, 1);
        BOOST_CHECK(!cbor::read_tag(in, tag));
        cbor::read(in, actual);
        BOOST_CHECK_EQUAL(actual, expected);
    }
}
BOOST_AUTO_TEST_CASE(container)
{
    using List = std::list<std::string>;
    using Map  = std::map<int, List>;

    Map data = {{1, {{"one"},{"two"},{"three"}}}, {2, {{"abc"},{"cde"}}}};
    cbor::binary buffer;
    cbor::omemstream out(buffer);
    cbor::write(out, data);

    Map actual;
    cbor::imemstream in(buffer);
    cbor::read(in, actual);
    BOOST_CHECK(data == actual);
}
BOOST_AUTO_TEST_CASE(proxy)
{
    auto sCall = cbor::make_proxy([](int a, std::string b, double c) -> float
    {
        BOOST_TEST_MESSAGE("args " << a << ", " << b << ", " << c);
        float rc = a + b.size() * c;
        BOOST_TEST_MESSAGE("return " << rc);
        return rc;
    });

    cbor::binary buffer;
    cbor::omemstream out(buffer);
    //cbor::write(out, 10, boost::string_ref("some"), 0.1);
    cbor::write(out, std::make_tuple(10, boost::string_ref("some"), 0.1));

    cbor::binary result;
    sCall->run(cbor::imemstream(buffer), cbor::omemstream(result));

    cbor::imemstream in(result);
    float result_value = 0;
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

        cbor::omemstream out(tmp);
        sTmp.write(out);
        BOOST_TEST_MESSAGE(Parser::to_hex(std::string(tmp.begin(), tmp.end())));
    }
    {
        cbor::imemstream in(tmp);
        test::Vary sTmp;
        sTmp.read(in);

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
BOOST_AUTO_TEST_SUITE_END()