#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include "Digest.hpp"
#include "GCM.hpp"
#include "HMAC.hpp"

BOOST_AUTO_TEST_SUITE(SSLxx)
BOOST_AUTO_TEST_CASE(hash)
{
    BOOST_CHECK_EQUAL(SSLxx::DigestStr(EVP_sha256(), std::string_view("qwerty")), "65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c5");
    BOOST_CHECK_EQUAL(SSLxx::DigestStr(EVP_md5(), std::string_view("qwerty")), "d8578edf8458ce06fbc5bb76a58c5ca4");
    BOOST_CHECK_EQUAL(SSLxx::DigestHash(EVP_md5(), std::string_view("qwerty")), 0xA45C8CA576BBC5FBULL);
    BOOST_CHECK_EQUAL(SSLxx::DigestNth(EVP_md5(), 10, std::string_view("qwerty10")), false);
    BOOST_CHECK_EQUAL(SSLxx::DigestNth(EVP_md5(), 10, std::string_view("qwerty99")), true);
    BOOST_CHECK_EQUAL(SSLxx::DigestStr(EVP_md5(), 42), "9824a7030ce67cf3f0efe7529f0c6ecc");
}
BOOST_AUTO_TEST_CASE(encrypt_aes_gcm)
{
    using namespace SSLxx::GCM;
    const std::string sMessage = "test123";
    const std::string sKey = SSLxx::Scrypt("123","salt", 32); // 256bit key for aes256
    Config sCfg{EVP_aes_256_gcm(), "0123456789ab", sKey}; // 96 bit iv
    auto sResult = Encrypt(sMessage, sCfg);
    BOOST_TEST_MESSAGE("key:       " << Format::to_hex(sKey));
    BOOST_TEST_MESSAGE("plain:     " << Format::to_hex(sMessage));
    BOOST_TEST_MESSAGE("encrypted: " << Format::to_hex(sResult.data));
    BOOST_TEST_MESSAGE("tag:       " << Format::to_hex(sResult.tag));
    auto sDecrypt = Decrypt(sResult, sCfg);
    BOOST_CHECK_EQUAL(sDecrypt, sMessage);
}
BOOST_AUTO_TEST_CASE(encrypt_chacha20_poly1305)
{
    using namespace SSLxx::GCM;
    const std::string sMessage = "test123";
    const std::string sKey = SSLxx::Scrypt("123","salt", 32); // 256bit key
    Config sCfg{EVP_chacha20_poly1305(), "0123456789ab", sKey}; // 96 bit iv
    auto sResult = Encrypt(sMessage, sCfg);
    BOOST_TEST_MESSAGE("key:       " << Format::to_hex(sKey));
    BOOST_TEST_MESSAGE("plain:     " << Format::to_hex(sMessage));
    BOOST_TEST_MESSAGE("encrypted: " << Format::to_hex(sResult.data));
    BOOST_TEST_MESSAGE("tag:       " << Format::to_hex(sResult.tag));
    auto sDecrypt = Decrypt(sResult, sCfg);
    BOOST_CHECK_EQUAL(sDecrypt, sMessage);
}
BOOST_AUTO_TEST_CASE(hmac)
{
    using namespace SSLxx::HMAC;

    const std::string sData = "qwerty";
    const Key sKey("secret");

    const std::string sTmp = Sign(EVP_sha256(), sKey, sData);
    BOOST_CHECK(Verify(EVP_sha256(), sKey, sTmp, sData));
    BOOST_CHECK_EQUAL(Verify(EVP_sha256(), sKey, sTmp, sData + "x"), false);
}
BOOST_AUTO_TEST_SUITE_END()
