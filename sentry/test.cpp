#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <gnu/libc-version.h>

#include "Message.hpp"
#include "Client.hpp"

Sentry::Trace bar() { return Sentry::GetTrace(); }
Sentry::Trace foo() { return bar(); }

BOOST_AUTO_TEST_SUITE(Sentry)
BOOST_AUTO_TEST_CASE(simple)
{
    Sentry::Message::Breadcrumb sWork;
    sWork.category="test";
    sWork.message="test started";
    sWork.timestamp=time(nullptr);
    sWork.aux["input"]="123";
    sWork.aux["operation"]="collect";

    Sentry::Message::Request sRequest;
    sRequest.url="https://docs.sentry.io/development/sdk-dev/event-payloads/request/";
    sRequest.data="Submitted data in a format that makes the most sense";

    auto sCurlVersion = curl_version_info(CURLVERSION_NOW);

    Sentry::Message sM;
    sM.set_environment("dev").
       set_transaction("8VSi1K2EX7QwcyzoVgvn7VXHREispL").
       set_message("fail to download").
       set_module("curl",sCurlVersion->version).
       set_module("ssl",sCurlVersion->ssl_version).
       set_module("libz",sCurlVersion->libz_version).
       set_module("glibc", gnu_get_libc_version()).
       set_tag("demo","yes").
       set_extra("filename","foobar").
       set_exception("Sentry::Error","some message").
       set_trace(foo()).
       log_work(sWork).
       set_user("alice@foo.bar","127.0.0.1").
       set_request(sRequest).
       set_version("test.cpp","0.1");

    BOOST_TEST_MESSAGE(sM.to_string());
}
BOOST_AUTO_TEST_CASE(client)
{
    Sentry::Client sClient;

    const auto sTrace = foo();
    Sentry::Message sMsg("main");
    sMsg.set_message("fail to create a shell");
    sMsg.set_exception("File not found", "/bin/bash");
    sMsg.set_trace(sTrace);
    sMsg.set_level("fatal");
    sMsg.set_environment("test");

    auto rc = sClient.send(sMsg);
    BOOST_CHECK_EQUAL(rc.status, 200);
    BOOST_TEST_MESSAGE(rc.body);
}
BOOST_AUTO_TEST_SUITE_END()
