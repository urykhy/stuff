#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Message.hpp"
#include "Client.hpp"

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
BOOST_AUTO_TEST_CASE(client)
{
    Sentry::Client::Params sParams;
    sParams.url="web.sentry.docker:9000/api/2/store/";
    sParams.key="626d891753d6489ba426baa41d7c79fc";
    sParams.secret="350776c0cfba4013a93275e9de63ba5d";
    Sentry::Client sClient(sParams);

    const auto sTrace = foo();
    Sentry::Message sMsg("main");
    sMsg.set_message("fail to create a shell");
    sMsg.set_exception("File not found", "/bin/bash");
    sMsg.set_trace(sTrace);
    sMsg.set_level("fatal");
    sMsg.set_environment("test");

    auto rc = sClient.send(sMsg);
    BOOST_CHECK_EQUAL(rc.first, 200);
    BOOST_TEST_MESSAGE(rc.second);
}
BOOST_AUTO_TEST_SUITE_END()