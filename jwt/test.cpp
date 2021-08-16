#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "JWT.hpp"

const Jwt::Manager sManager("secret");

BOOST_AUTO_TEST_SUITE(JWT)
BOOST_AUTO_TEST_CASE(basic)
{
    Jwt::Claim sClaim{.exp      = time(nullptr) + 10,
                      .nbf      = time(nullptr) - 10,
                      .iss      = "iss",
                      .aud      = "aud",
                      .username = "test"};
    BOOST_TEST_MESSAGE(Format::Json::to_string(Format::Json::to_value(sClaim)));

    const std::string  sToken = sManager.Sign(sClaim);
    BOOST_TEST_MESSAGE("token: " << sToken);

    const auto sResult = sManager.Validate(sToken);
    BOOST_CHECK_EQUAL(sClaim.iss, sResult.iss);
    BOOST_CHECK_EQUAL(sClaim.aud, sResult.aud);
    BOOST_CHECK_EQUAL(sClaim.username, sResult.username);
}
BOOST_AUTO_TEST_CASE(nbf)
{
    Jwt::Claim sClaim{.exp      = time(nullptr) + 10,
                      .nbf      = time(nullptr) + 10, // nbf in future
                      .iss      = "iss",
                      .aud      = "aud",
                      .username = "test"};
    const std::string  sToken = sManager.Sign(sClaim);
    BOOST_CHECK_THROW(sManager.Validate(sToken), Jwt::Manager::Error);
}
BOOST_AUTO_TEST_CASE(exp)
{
    Jwt::Claim sClaim{.exp      = time(nullptr) - 10, // expired in the past
                      .nbf      = time(nullptr) - 10,
                      .iss      = "iss",
                      .aud      = "aud",
                      .username = "test"};
    const std::string  sToken = sManager.Sign(sClaim);
    BOOST_CHECK_THROW(sManager.Validate(sToken), Jwt::Manager::Error);
}
BOOST_AUTO_TEST_SUITE_END()