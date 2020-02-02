#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Message.hpp"
#include "Client.hpp"

Sentry::Trace bar() { return Sentry::GetTrace(); }
Sentry::Trace foo() { return bar(); }

BOOST_AUTO_TEST_SUITE(Sentry)
BOOST_AUTO_TEST_CASE(simple)
{
    BOOST_TEST_MESSAGE("uuid " << Util::Uuid());
    BOOST_TEST_MESSAGE(Sentry::Message().to_string());
    BOOST_TEST_MESSAGE(Sentry::Message().set_tag("exception_type","std::test").to_string());
    BOOST_TEST_MESSAGE(Sentry::Message().set_message("fail to download").set_extra("filename","foobar").to_string());

    const auto sBT = foo();
    BOOST_TEST_MESSAGE(Sentry::Message().set_exception("ValueError", "44").set_trace(sBT).to_string());

    Sentry::Message::Breadcrumb sWork;
    sWork.category="test";
    sWork.message="test started";
    sWork.timestamp=time(nullptr);
    sWork.aux["input"]="123";
    sWork.aux["operation"]="collect";
    BOOST_TEST_MESSAGE(Sentry::Message().log_work(sWork).set_message("multiple numbers required").to_string());

    BOOST_TEST_MESSAGE(Sentry::Message().set_user("alice@foo.bar","127.0.0.1").set_message("user not found").to_string());

    Sentry::Message::Request sRequest;
    sRequest.url="https://docs.sentry.io/development/sdk-dev/event-payloads/request/";
    sRequest.aux["data"]="Submitted data in a format that makes the most sense";
    BOOST_TEST_MESSAGE(Sentry::Message().set_request(sRequest).set_message("request failed").to_string());

    BOOST_TEST_MESSAGE(Sentry::Message().set_version("test.cpp","0.1").set_message("test ok").set_level("info").to_string());
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