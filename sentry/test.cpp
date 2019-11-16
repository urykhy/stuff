#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Message.hpp"

Util::Stacktrace bar() { return Util::GetStacktrace(); }
Util::Stacktrace foo() { return bar(); }

BOOST_AUTO_TEST_SUITE(Sentry)
BOOST_AUTO_TEST_CASE(simple)
{
    BOOST_TEST_MESSAGE("uuid " << Util::Uuid());
    BOOST_TEST_MESSAGE(Sentry::Message().to_string());
    BOOST_TEST_MESSAGE(Sentry::Message().set_tag("exception_type","std::test").to_string());
    BOOST_TEST_MESSAGE(Sentry::Message().set_message("fail to download").to_string());

    const auto sBT = foo();
    BOOST_TEST_MESSAGE(Sentry::Message().set_exception("ValueError", "44").set_trace(sBT).to_string());
}
BOOST_AUTO_TEST_SUITE_END()