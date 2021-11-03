#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Asymm.hpp"
#include "CTR.hpp"
#include "Digest.hpp"
#include "GCM.hpp"
#include "HMAC.hpp"

#include <parser/Hex.hpp>

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
BOOST_AUTO_TEST_CASE(encrypt_aes_ctr)
{
    using namespace SSLxx::CTR;
    const std::string sMessage = "test123";
    const std::string sKey     = SSLxx::Scrypt("123", "salt", 16);       // 128bit key for aes 128
    SSLxx::Config     sCfg{EVP_aes_128_ctr(), "0123456789abcdef", sKey}; // 128 bit iv

    auto sResult = Encrypt(sCfg, sMessage);
    BOOST_TEST_MESSAGE("key:       " << Format::to_hex(sKey));
    BOOST_TEST_MESSAGE("iv:        " << Format::to_hex(sCfg.iv));
    BOOST_TEST_MESSAGE("plain:     " << Format::to_hex(sMessage));
    BOOST_TEST_MESSAGE("encrypted: " << Format::to_hex(sResult));
    auto sDecrypt = Decrypt(sCfg, sResult);
    BOOST_CHECK_EQUAL(sDecrypt, sMessage);
}
BOOST_AUTO_TEST_CASE(aes_ctr_calculate_iv)
{
    using namespace SSLxx::CTR;
    const std::string sKey = SSLxx::Scrypt("123", "salt", 16); // 128bit key for aes 128
    const std::string sIV("\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xf5", 16);

    auto sResult = Encrypt(SSLxx::Config{EVP_aes_128_ctr(), sIV, sKey}, "Begin some not very long message");  // 32 bytes
    auto sPart1  = Encrypt(SSLxx::Config{EVP_aes_128_ctr(), sIV, sKey}, "Begin some not v");                  // 16 bytes
    auto sPart2  = Encrypt(SSLxx::Config{EVP_aes_128_ctr(), calculateIV(sIV, 16), sKey}, "ery long message"); // 16 bytes
    BOOST_CHECK_EQUAL(Format::to_hex(sResult), Format::to_hex(sPart1 + sPart2));
}
BOOST_AUTO_TEST_CASE(encrypt_aes_gcm)
{
    using namespace SSLxx::GCM;
    const std::string sMessage = "test123";
    const std::string sKey     = SSLxx::Scrypt("123", "salt", 32);   // 256bit key for aes256
    SSLxx::Config     sCfg{EVP_aes_256_gcm(), "0123456789ab", sKey}; // 96 bit iv
    auto              sResult = Encrypt(sCfg, sMessage);
    BOOST_TEST_MESSAGE("key:       " << Format::to_hex(sKey));
    BOOST_TEST_MESSAGE("iv:        " << Format::to_hex(sCfg.iv));
    BOOST_TEST_MESSAGE("plain:     " << Format::to_hex(sMessage));
    BOOST_TEST_MESSAGE("encrypted: " << Format::to_hex(sResult.data));
    BOOST_TEST_MESSAGE("tag:       " << Format::to_hex(sResult.tag));
    auto sDecrypt = Decrypt(sCfg, sResult);
    BOOST_CHECK_EQUAL(sDecrypt, sMessage);
}
BOOST_AUTO_TEST_CASE(encrypt_chacha20_poly1305)
{
    using namespace SSLxx::GCM;
    const std::string sMessage = "test123";
    const std::string sKey     = SSLxx::Scrypt("123", "salt", 32);         // 256bit key
    SSLxx::Config     sCfg{EVP_chacha20_poly1305(), "0123456789ab", sKey}; // 96 bit iv
    auto              sResult = Encrypt(sCfg, sMessage);
    BOOST_TEST_MESSAGE("key:       " << Format::to_hex(sKey));
    BOOST_TEST_MESSAGE("iv:        " << Format::to_hex(sCfg.iv));
    BOOST_TEST_MESSAGE("plain:     " << Format::to_hex(sMessage));
    BOOST_TEST_MESSAGE("encrypted: " << Format::to_hex(sResult.data));
    BOOST_TEST_MESSAGE("tag:       " << Format::to_hex(sResult.tag));
    auto sDecrypt = Decrypt(sCfg, sResult);
    BOOST_CHECK_EQUAL(sDecrypt, sMessage);
}
BOOST_AUTO_TEST_CASE(hmac)
{
    using namespace SSLxx::HMAC;

    const std::string sData = "qwerty";
    const auto        sKey  = hmacKey("secret");

    const std::string sTmp = Sign(EVP_sha256(), sKey, sData);
    BOOST_CHECK(Verify(EVP_sha256(), sKey, sTmp, sData));
    BOOST_CHECK_EQUAL(Verify(EVP_sha256(), sKey, sTmp, sData + "x"), false);
}
BOOST_AUTO_TEST_CASE(key)
{
    // openssl ecparam -name prime256v1 -genkey -noout -out key.pem
    // openssl ec -in key.pem -pubout -out public.pem
    // openssl ec -in key.pem -text -noout
    const std::string_view sPrivate =
        "-----BEGIN EC PRIVATE KEY-----\n"
        "MHcCAQEEIBKnJ8wdML8agKcKOFxYZEnjliifVaeJ2NjxJSuI58QHoAoGCCqGSM49\n"
        "AwEHoUQDQgAEDWDVvQDnxtYG03NcAgBw4/DxKF6VDSD3S1lzY38LCpfSiGLKTfgU\n"
        "MS+4VR39qmdTdp6Cs2bx+Y8WgQ6dW5mLuA==\n"
        "-----END EC PRIVATE KEY-----\n";
    const std::string_view sPublic =
        "-----BEGIN PUBLIC KEY-----\n"
        "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEDWDVvQDnxtYG03NcAgBw4/DxKF6V\n"
        "DSD3S1lzY38LCpfSiGLKTfgUMS+4VR39qmdTdp6Cs2bx+Y8WgQ6dW5mLuA==\n"
        "-----END PUBLIC KEY-----\n";

    auto sKey = SSLxx::privateKey(sPrivate);
    BOOST_CHECK(sKey);
    auto sPubKey = SSLxx::publicKey(sPublic);
    BOOST_CHECK(sPubKey);

    // echo -n "12345" | openssl dgst -sha256 -sign key.pem > signature.bin
    // echo -n "12345" | openssl dgst -sha256 -verify public.pem -signature signature.bin

    std::string sData      = "12345";
    std::string sSignature = SSLxx::Asymm::Sign(EVP_sha256(), sKey.get(), sData);
    BOOST_TEST_MESSAGE("signature: " << Format::to_hex(sSignature));
    BOOST_CHECK(SSLxx::Asymm::Verify(EVP_sha256(), sPubKey.get(), sSignature, sData));

    // test with signature from signature.bin
    sSignature = Parser::from_hex("3045022100bb6d810217ba40f5b7a09b5c81c7e7e0007173d24064f06a2802aeb8850036c002203e1b6fadded87fc3473b170c196042b5ffd36fdbf0cdc74e2871085fbe4765e0");
    BOOST_CHECK(SSLxx::Asymm::Verify(EVP_sha256(), sPubKey.get(), sSignature, sData));
}
BOOST_AUTO_TEST_SUITE_END()
