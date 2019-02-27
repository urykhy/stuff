#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <SSL.hpp>

BOOST_AUTO_TEST_SUITE(SSLxx)
BOOST_AUTO_TEST_CASE(hash)
{
    BOOST_CHECK_EQUAL(SSLxx::DigestStr("qwerty", EVP_sha256()), "65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c5");
    BOOST_CHECK_EQUAL(SSLxx::DigestStr("qwerty", EVP_md5()), "d8578edf8458ce06fbc5bb76a58c5ca4");
}
BOOST_AUTO_TEST_CASE(encrypt_aes_gcm)
{
    using namespace SSLxx::GCM;
    const std::string sMessage = "test123";
    const std::string sKey = SSLxx::Scrypt("123","salt", 32); // 256bit key for aes256
    Config sCfg{EVP_aes_256_gcm(), "0123456789abcdef", sKey};
    auto sResult = Encrypt(sMessage, sCfg);
    BOOST_TEST_MESSAGE("key:       " << Parser::to_hex(sKey));
    BOOST_TEST_MESSAGE("plain:     " << Parser::to_hex(sMessage));
    BOOST_TEST_MESSAGE("encrypted: " << Parser::to_hex(sResult.data));
    BOOST_TEST_MESSAGE("tag:       " << Parser::to_hex(sResult.tag));
    auto sDecrypt = Decrypt(sResult, sCfg);
    BOOST_CHECK_EQUAL(sDecrypt, sMessage);
}
BOOST_AUTO_TEST_SUITE_END()
