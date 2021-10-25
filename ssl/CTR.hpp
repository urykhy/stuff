#pragma once

#include <string>
#include <string_view>

#include "Util.hpp"

namespace SSLxx::CTR {

    inline std::string Encrypt(const Config& sCfg, const std::string_view aInput)
    {
        std::string sResult;
        sResult.resize(aInput.size());

        int sCiphertextLen = 0;
        int sLen           = 0;

        auto sCtx = makeCipherCtx();
        if (1 != EVP_EncryptInit_ex(sCtx.get(), sCfg.kind, NULL, (const uint8_t*)sCfg.key.data(), (const uint8_t*)sCfg.iv.data()))
            throw Error("EVP_EncryptInit_ex");

        if (1 != EVP_EncryptUpdate(sCtx.get(), (uint8_t*)sResult.data(), &sCiphertextLen, (const uint8_t*)aInput.data(), aInput.size()))
            throw Error("EVP_EncryptUpdate");

        if (1 != EVP_EncryptFinal_ex(sCtx.get(), (uint8_t*)sResult.data() + sCiphertextLen, &sLen))
            throw Error("EVP_EncryptFinal_ex");

        if ((int)sResult.size() != sCiphertextLen + sLen)
            throw Error("EVP_Encrypt bad size");

        return sResult;
    }

    inline std::string Decrypt(const Config& sCfg, const std::string_view& aInput)
    {
        std::string sResult;
        sResult.resize(aInput.size());

        int sPlaintextLen = 0;
        int sLen          = 0;

        auto sCtx = makeCipherCtx();
        if (1 != EVP_DecryptInit_ex(sCtx.get(), sCfg.kind, NULL, (const uint8_t*)sCfg.key.data(), (const uint8_t*)sCfg.iv.data()))
            throw Error("EVP_DecryptInit_ex");

        if (1 != EVP_DecryptUpdate(sCtx.get(), (uint8_t*)sResult.data(), &sPlaintextLen, (const uint8_t*)aInput.data(), aInput.size()))
            throw Error("EVP_EncryptUpdate");

        if (1 != EVP_DecryptFinal_ex(sCtx.get(), NULL, &sLen))
            throw Error("EVP_DecryptFinal_ex/fail to decrypt");

        if ((int)sResult.size() != sPlaintextLen + sLen)
            throw Error("EVP_Decrypt bad size");

        return sResult;
    }
} // namespace SSLxx::CTR
