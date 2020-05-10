#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include <endian.h>
#include <parser/Hex.hpp>
#include <unsorted/Random.hpp>
#include <container/Stream.hpp>

#include "Digest.hpp"
#include "Password.hpp"
#include "Crypto.hpp"

const std::string IV12{"0123456789ab"};
const std::string IV16{"0123456789abcdef"};

const std::string PASSSWORD16{"secure_password1"};

BOOST_AUTO_TEST_SUITE(af_alg)
BOOST_AUTO_TEST_CASE(random)
{
    Util::Random sRND;
    const auto sTmp = sRND(12);
    BOOST_CHECK_EQUAL(sTmp.size(), 12);
    BOOST_TEST_MESSAGE("random: " << Parser::to_hex(sTmp));
}

BOOST_AUTO_TEST_SUITE(digest)
BOOST_AUTO_TEST_CASE(md5)
{   // echo -n "qwerty" | md5sum
    af_alg::DigestImpl sMD5("md5", 16);
    auto sResult = af_alg::Digest(sMD5, std::string("qwe"), std::string("rty"));
    BOOST_CHECK_EQUAL(Parser::to_hex(sResult), "d8578edf8458ce06fbc5bb76a58c5ca4");
    // reuse impl
    // echo -n "abcdef" | md5sum
    sResult = af_alg::Digest(sMD5, std::string("abcdef"));
    BOOST_CHECK_EQUAL(Parser::to_hex(sResult), "e80b5017098950fc58aad83c8c14978e");
}
BOOST_AUTO_TEST_CASE(hmac_sha256)
{   // echo -n "qwerty" | openssl dgst -sha256 -hmac some_pass
    af_alg::DigestImpl sHmac("hmac(sha256)", 32, "some_pass");
    auto sResult = af_alg::Digest(sHmac, std::string("qwerty"));
    BOOST_CHECK_EQUAL(Parser::to_hex(sResult), "03353640f3645d2356f29ba368951f9be22af36ae09e7314f1163ac81e38ed84");
}
BOOST_AUTO_TEST_CASE(cmac_aes)
{   // echo -n qwerty | openssl dgst -mac cmac -macopt cipher:aes-128-cbc -macopt hexkey:`echo -n secure_password1 | xxd -p`
    af_alg::DigestImpl sMac("cmac(aes)", 16, PASSSWORD16);
    auto sResult = af_alg::Digest(sMac, std::string("qwerty"));
    BOOST_CHECK_EQUAL(Parser::to_hex(sResult), "3588bcb162ca6fa7e5854e37d90e2df3");
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(password)
{
    const std::string sPassword="qwerty";

    af_alg::PasswordConfig sCfg{"hmac(sha256)", PASSSWORD16, 32};
    af_alg::Password sPass(sCfg);

    const std::string sHash = sPass.create(sPassword);
    BOOST_TEST_MESSAGE("hashed password: " << Parser::to_hex(sHash));
    BOOST_CHECK(sPass.validate(sPassword, sHash));
    BOOST_CHECK_EQUAL(sPass.validate("wrong password", sHash), false);
}

BOOST_AUTO_TEST_SUITE(encrypt)
BOOST_AUTO_TEST_CASE(ctr_aes)
{
    // min keysize  : 16
    // max keysize  : 32
    // ivsize       : 16
    const std::string sInput("qwerty");
    af_alg::CryptoCfg sCfg{"ctr(aes)", IV16, PASSSWORD16, true};
    af_alg::CryptoImpl sImpl(sCfg);
    const std::string sEncrypted = sImpl(sInput);
    BOOST_TEST_MESSAGE("encrypted: " << Parser::to_hex(sEncrypted));

    sCfg.encrypt = false;
    af_alg::CryptoImpl sImplD(sCfg);
    const std::string sDecrypted = sImplD(sEncrypted);
    BOOST_CHECK_EQUAL(sDecrypted, sInput);
}
BOOST_AUTO_TEST_CASE(gcm_aes)
{
    // ivsize       : 12
    // maxauthsize  : 16
    const std::string sInput("qwerty");
    af_alg::CryptoCfg sCfg{"gcm(aes)", IV12, PASSSWORD16, true, 8};
    af_alg::CryptoImpl sImpl(sCfg);
    const std::string sEncrypted = sImpl(sInput);
    BOOST_TEST_MESSAGE("encrypted: " << Parser::to_hex(sEncrypted));

    sCfg.encrypt = false;
    af_alg::CryptoImpl sImplD(sCfg);
    const std::string sDecrypted = sImplD(sEncrypted);
    BOOST_CHECK_EQUAL(sDecrypted, sInput);
}
BOOST_AUTO_TEST_CASE(auth_enc)
{
    // min keysize  : 16
    // blocksize    : 16
    // ivsize       : 16
    // maxauthsize  : 20
    const std::string sInput("one 16byte block");

    // authenc.c#crypto_authenc_extractkeys
    std::string sKey;
    Container::omemstream sBuilder(sKey);
    sBuilder.write((uint16_t)8);    // rta_len
    sBuilder.write((uint16_t)1);    // rta_type CRYPTO_AUTHENC_KEYA_PARAM
    sBuilder.write(htobe32(PASSSWORD16.size()));    // enckeylen
    sKey.append(PASSSWORD16);

    af_alg::CryptoCfg sCfg{"authenc(hmac(sha1),cbc(aes))", IV16, sKey, true, 8};
    af_alg::CryptoImpl sImpl(sCfg);
    const std::string sEncrypted = sImpl(sInput);
    BOOST_TEST_MESSAGE("encrypted: " << Parser::to_hex(sEncrypted));

    sCfg.encrypt = false;
    af_alg::CryptoImpl sImplD(sCfg);
    const std::string sDecrypted = sImplD(sEncrypted);
    BOOST_CHECK_EQUAL(sDecrypted, sInput);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
