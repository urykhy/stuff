#pragma once

#include <openssl/evp.h>

#include <cassert>
#include <string>
#include <iostream>

#include <../unsorted/Raii.hpp>
#include <../parser/Hex.hpp>

namespace SSLxx
{
    using Error = std::runtime_error;

    // aKind is a EVP_sha256() for example
    inline std::string Digest(const std::string& aInput, const EVP_MD* aKind)
    {
        unsigned int sLen = EVP_MD_size(aKind);
        std::string sResult;
        sResult.resize(sLen);

        EVP_MD_CTX* sCtx = nullptr;
        Util::Raii sCleanup([&sCtx](){
            if (sCtx != nullptr)
                EVP_MD_CTX_destroy(sCtx);
        });

        sCtx = EVP_MD_CTX_create();
        if (sCtx == nullptr) throw Error("EVP_MD_CTX_create");
        if (1 != EVP_DigestInit_ex(sCtx, aKind, NULL)) throw Error("EVP_DigestInit_ex");
        if (1 != EVP_DigestUpdate(sCtx, aInput.data(), aInput.size())) throw Error("EVP_DigestUpdate");
        if (1 != EVP_DigestFinal_ex(sCtx, (uint8_t*)sResult.data(), &sLen)) throw Error("EVP_DigestFinal_ex");

        assert(sLen == sResult.size());

        return sResult;
    }

    inline std::string DigestStr(const std::string& aInput, const EVP_MD* aKind)
    {
        return Parser::to_hex(Digest(aInput, aKind));
    }

    inline uint64_t DigestHash(const std::string& aInput, const EVP_MD* aKind)
    {
        const auto sData = Digest(aInput, aKind);
        const char* sPtr = sData.data() + sData.size() - sizeof(uint64_t);
        return *reinterpret_cast<const uint64_t*>(sPtr);
    }

    inline bool DigestNth(const std::string& aInput, const EVP_MD* aKind, uint64_t aPart)
    {
        return DigestHash(aInput, aKind) % aPart == 0;
    }

    inline std::string Scrypt(const std::string& aPass, const std::string& aSalt, size_t aSize)
    {
        std::string sResult(aSize, ' ');

        if (1 != EVP_PBE_scrypt(aPass.data(), aPass.size(),
                               (const uint8_t*)aSalt.data(), aSalt.size(),
                               1024, 8, 1, 0,
                               (uint8_t*)sResult.data(), sResult.size()))
            Error("EVP_PBE_scrypt");

        return sResult;
    }

namespace GCM
{
    struct Config
    {
        const EVP_CIPHER* m_Kind = nullptr; // EVP_aes_256_gcm
        std::string m_IV;
        std::string m_Key;
    };

    struct Result
    {
        std::string data;
        std::string tag;
    };

    inline Result Encrypt(const std::string& aInput, const Config& sCfg)
    {
        std::string sResult;
        sResult.resize(aInput.size());
        std::string sTag;
        sTag.resize(EVP_GCM_TLS_TAG_LEN);

        int sCiphertextLen = 0;
        int sAuthLen = 0;

        EVP_CIPHER_CTX *sCtx = nullptr;
        Util::Raii sCleanup([&sCtx](){
            if (sCtx != nullptr)
                EVP_CIPHER_CTX_free(sCtx);
        });

        if(!(sCtx = EVP_CIPHER_CTX_new())) throw Error("EVP_CIPHER_CTX_new");
        if(1 != EVP_EncryptInit_ex(sCtx, sCfg.m_Kind, NULL, NULL, NULL)) throw Error("EVP_EncryptInit_ex");
        if(1 != EVP_CIPHER_CTX_ctrl(sCtx, EVP_CTRL_GCM_SET_IVLEN, sCfg.m_IV.size(), NULL)) throw Error("EVP_CIPHER_CTX_ctrl/IV");
        if(1 != EVP_EncryptInit_ex(sCtx, NULL, NULL, (const uint8_t*)sCfg.m_Key.data(), (const uint8_t*)sCfg.m_IV.data())) throw Error("EVP_EncryptInit_ex");
        if(1 != EVP_EncryptUpdate(sCtx, (uint8_t*)sResult.data(), &sCiphertextLen, (const uint8_t*)aInput.data(), aInput.size())) throw Error("EVP_EncryptUpdate");
        if(1 != EVP_EncryptFinal_ex(sCtx, (uint8_t*)sResult.data() + sCiphertextLen, &sAuthLen)) throw Error("EVP_EncryptFinal_ex");
        if(1 != EVP_CIPHER_CTX_ctrl(sCtx, EVP_CTRL_GCM_GET_TAG, sTag.size(), (uint8_t*)sTag.data()))  throw Error("EVP_CIPHER_CTX_ctrl");

        sResult.resize(sCiphertextLen + sAuthLen);

        return Result{std::move(sResult), std::move(sTag)};
    }

    inline std::string Decrypt(const Result& aInput, const Config& sCfg)
    {
        const auto& sData = aInput.data;
        auto sTag = aInput.tag;

        std::string sResult;
        sResult.resize(sData.size());
        int sPlaintextLen = 0;
        int sLen = 0;

        EVP_CIPHER_CTX *sCtx = nullptr;
        Util::Raii sCleanup([&sCtx](){
            if (sCtx != nullptr)
                EVP_CIPHER_CTX_free(sCtx);
        });

        if(!(sCtx = EVP_CIPHER_CTX_new())) throw Error("EVP_CIPHER_CTX_new");
        if(1 != EVP_DecryptInit_ex(sCtx, sCfg.m_Kind, NULL, NULL, NULL))throw Error("EVP_DecryptInit_ex");
        if(1 != EVP_CIPHER_CTX_ctrl(sCtx, EVP_CTRL_GCM_SET_IVLEN, sCfg.m_IV.size(), NULL)) throw Error("EVP_CIPHER_CTX_ctrl/IV");
        if(1 != EVP_DecryptInit_ex(sCtx, NULL, NULL, (const uint8_t*)sCfg.m_Key.data(), (const uint8_t*)sCfg.m_IV.data())) throw Error("EVP_DecryptInit_ex");
        if(1 != EVP_DecryptUpdate(sCtx, (uint8_t*)sResult.data(), &sPlaintextLen, (const uint8_t*)sData.data(), sData.size())) throw Error("EVP_EncryptUpdate");
        if(1 != EVP_CIPHER_CTX_ctrl(sCtx, EVP_CTRL_GCM_SET_TAG, sTag.size(), (uint8_t*)sTag.data())) throw Error("EVP_CIPHER_CTX_ctrl");
        if(1 != EVP_DecryptFinal_ex(sCtx, NULL, &sLen)) throw Error("EVP_DecryptFinal_ex/fail to decrypt");
        sResult.resize(sPlaintextLen);

        return sResult;
    }
} // GCM

namespace HMAC
{
    class Key
    {
        EVP_PKEY *m_Key = nullptr;
    public:
        Key(const std::string& aKey) : m_Key(EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, (const uint8_t*)aKey.data(), aKey.size()))
        {
            if (m_Key == nullptr)
                throw Error("EVP_PKEY_new_mac_key");
        }

        ~Key() { EVP_PKEY_free(m_Key); }
        operator EVP_PKEY* () const { return m_Key; }
    };

    // EVP_sha256()
    inline std::string Sign(const std::string& aInput, const Key& aKey, const EVP_MD* aKind)
    {
        size_t sLen = EVP_MD_size(aKind);
        std::string sResult;
        sResult.resize(sLen);

        EVP_MD_CTX* sCtx = nullptr;
        Util::Raii sCleanup([&sCtx](){ if (sCtx != nullptr) EVP_MD_CTX_destroy(sCtx); });
        sCtx = EVP_MD_CTX_create();
        if (sCtx == nullptr) throw Error("EVP_MD_CTX_create");

        if (1 != EVP_DigestInit_ex(sCtx, aKind, NULL)) throw Error("EVP_DigestInit_ex");
        if (1 != EVP_DigestSignInit(sCtx, NULL, aKind, NULL, aKey)) throw Error("EVP_DigestSignInit");
        if (1 != EVP_DigestSignUpdate(sCtx, aInput.data(), aInput.size())) throw Error("EVP_DigestSignUpdate");
        if (1 != EVP_DigestSignFinal(sCtx, (uint8_t*)sResult.data(), &sLen)) throw Error("EVP_DigestSignFinal");

        assert(sLen == sResult.size());

        return sResult;
    }

    bool Verify(const std::string& aInput, const std::string& aHash, const Key& aKey, const EVP_MD* aKind)
    {
        const std::string sHash = Sign(aInput, aKey, aKind);
        assert(sHash.size() == aHash.size());
        return 0 == CRYPTO_memcmp(sHash.data(), aHash.data(), aHash.size());
    }
} // HMAC

}
