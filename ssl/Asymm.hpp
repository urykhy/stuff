#pragma once

#include "Util.hpp"

namespace SSLxx::Asymm {

    namespace {
        inline void updateSign(EVP_MD_CTX* sCtx, std::string_view aInput)
        {
            if (1 != EVP_DigestSignUpdate(sCtx, aInput.data(), aInput.size()))
                throw Error("EVP_DigestSignUpdate");
        }

        template <class T>
        typename std::enable_if<std::is_integral_v<T>, void>::type updateSign(EVP_MD_CTX* sCtx, const T& aInput)
        {
            if (1 != EVP_DigestSignUpdate(sCtx, &aInput, sizeof(T)))
                throw Error("EVP_DigestSignUpdate");
        }

        inline void updateVerify(EVP_MD_CTX* sCtx, std::string_view aInput)
        {
            if (1 != EVP_DigestVerifyUpdate(sCtx, aInput.data(), aInput.size()))
                throw Error("EVP_DigestVerifyUpdate");
        }

        template <class T>
        typename std::enable_if<std::is_integral_v<T>, void>::type updateVerify(EVP_MD_CTX* sCtx, const T& aInput)
        {
            if (1 != EVP_DigestVerifyUpdate(sCtx, &aInput, sizeof(T)))
                throw Error("EVP_DigestVerifyUpdate");
        }

    } // namespace

    // aKind is a EVP_sha256() for example
    template <class... T>
    inline std::string Sign(const EVP_MD* aKind, EVP_PKEY* aKey, T&&... aInput)
    {
        auto sCtx = makeDigestCtx();

        if (1 != EVP_DigestSignInit(sCtx.get(), NULL, aKind, NULL, aKey))
            throw Error("EVP_DigestSignInit");
        Mpl::for_each_argument(
            [&](const auto& aInput) {
                updateSign(sCtx.get(), aInput);
            },
            aInput...);

        size_t sLen = 0;
        if (1 != EVP_DigestSignFinal(sCtx.get(), NULL, &sLen))
            throw Error("EVP_DigestSignFinal_ex");

        std::string sResult(sLen, '\0');
        if (1 != EVP_DigestSignFinal(sCtx.get(), (uint8_t*)sResult.data(), &sLen))
            throw Error("EVP_DigestSignFinal_ex");
        sResult.resize(sLen);
        return sResult;
    }

    template <class... T>
    bool Verify(const EVP_MD* aKind, const std::string& aSignature, EVP_PKEY* aKey, T&&... aInput)
    {
        auto sCtx = makeDigestCtx();

        if (1 != EVP_DigestVerifyInit(sCtx.get(), NULL, aKind, NULL, aKey))
            throw Error("EVP_DigestVerifyInit");

        Mpl::for_each_argument(
            [&](const auto& aInput) {
                updateVerify(sCtx.get(), aInput);
            },
            aInput...);

        return 1 == EVP_DigestVerifyFinal(sCtx.get(), (const uint8_t*)aSignature.data(), aSignature.size());
    }

} // namespace SSLxx::Asymm
