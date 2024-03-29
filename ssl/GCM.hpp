#pragma once

#include <string>
#include <string_view>

#include "Util.hpp"

namespace SSLxx::GCM {

    struct Data
    {
        std::string data;
        std::string tag;
    };

    inline Data Encrypt(const Config& sCfg, const std::string_view aInput)
    {
        std::string sResult;
        sResult.resize(aInput.size());
        std::string sTag;
        sTag.resize(EVP_GCM_TLS_TAG_LEN);

        int sCiphertextLen = 0;
        int sAuthLen       = 0;

        auto sCtx = makeCipherCtx();
        if (1 != EVP_EncryptInit_ex(sCtx.get(), sCfg.kind, NULL, (const uint8_t*)sCfg.key.data(), (const uint8_t*)sCfg.iv.data()))
            throw Error("EVP_EncryptInit_ex");

        if (1 != EVP_EncryptUpdate(sCtx.get(), (uint8_t*)sResult.data(), &sCiphertextLen, (const uint8_t*)aInput.data(), aInput.size()))
            throw Error("EVP_EncryptUpdate");

        if (1 != EVP_EncryptFinal_ex(sCtx.get(), (uint8_t*)sResult.data() + sCiphertextLen, &sAuthLen))
            throw Error("EVP_EncryptFinal_ex");

        if (1 != EVP_CIPHER_CTX_ctrl(sCtx.get(), EVP_CTRL_GCM_GET_TAG, sTag.size(), (uint8_t*)sTag.data()))
            throw Error("EVP_CIPHER_CTX_ctrl");

        sResult.resize(sCiphertextLen + sAuthLen);

        return Data{std::move(sResult), std::move(sTag)};
    }

    inline std::string Decrypt(const Config& sCfg, const Data& aInput)
    {
        std::string sResult;
        sResult.resize(aInput.data.size());
        int sPlaintextLen = 0;
        int sLen          = 0;

        auto sCtx = makeCipherCtx();
        if (1 != EVP_DecryptInit_ex(sCtx.get(), sCfg.kind, NULL, (const uint8_t*)sCfg.key.data(), (const uint8_t*)sCfg.iv.data()))
            throw Error("EVP_DecryptInit_ex");

        if (1 != EVP_DecryptUpdate(sCtx.get(), (uint8_t*)sResult.data(), &sPlaintextLen, (const uint8_t*)aInput.data.data(), aInput.data.size()))
            throw Error("EVP_EncryptUpdate");

        if (1 != EVP_CIPHER_CTX_ctrl(sCtx.get(), EVP_CTRL_GCM_SET_TAG, aInput.tag.size(), (uint8_t*)aInput.tag.data()))
            throw Error("EVP_CIPHER_CTX_ctrl");

        if (1 != EVP_DecryptFinal_ex(sCtx.get(), NULL, &sLen))
            throw Error("EVP_DecryptFinal_ex/fail to decrypt");

        sResult.resize(sPlaintextLen + sLen);

        return sResult;
    }
} // namespace SSLxx::GCM
