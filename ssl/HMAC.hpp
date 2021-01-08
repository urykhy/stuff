#pragma once

#include <openssl/evp.h>

#include <cassert>
#include <string>
#include <string_view>

#include "Util.hpp"

namespace SSLxx::HMAC {

    // EVP_sha256()
    template <class... T>
    inline std::string Sign(const EVP_MD* aKind, const KeyPtr& aKey, T&&... aInput)
    {
        size_t      sLen = EVP_MD_size(aKind);
        std::string sResult;
        sResult.resize(sLen);

        auto sCtx = makeDigestCtx();
        if (1 != EVP_DigestInit_ex(sCtx.get(), aKind, NULL))
            throw Error("EVP_DigestInit_ex");

        if (1 != EVP_DigestSignInit(sCtx.get(), NULL, aKind, NULL, aKey.get()))
            throw Error("EVP_DigestSignInit");

        Mpl::for_each_argument(
            [&](const auto& aInput) {
                if (1 != EVP_DigestSignUpdate(sCtx.get(), aInput.data(), aInput.size()))
                    throw Error("EVP_DigestSignUpdate");
            },
            aInput...);

        if (1 != EVP_DigestSignFinal(sCtx.get(), (uint8_t*)sResult.data(), &sLen))
            throw Error("EVP_DigestSignFinal");

        assert(sLen == sResult.size());

        return sResult;
    }

    template <class... T>
    bool Verify(const EVP_MD* aKind, const KeyPtr& aKey, const std::string_view aHash, T&&... aInput)
    {
        const std::string sHash = Sign(aKind, aKey, std::forward<T&&>(aInput)...);
        assert(sHash.size() == aHash.size());
        return 0 == CRYPTO_memcmp(sHash.data(), aHash.data(), aHash.size());
    }
} // namespace SSLxx::HMAC
