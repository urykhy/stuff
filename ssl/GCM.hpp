#pragma once

#include <openssl/evp.h>
#include <string_view>
#include <unsorted/Raii.hpp>

namespace SSLxx::GCM
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

    inline Result Encrypt(const std::string_view aInput, const Config& sCfg)
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
