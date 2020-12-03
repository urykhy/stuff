#pragma once

#include <cassert>
#include <string>
#include <string_view>

#include <format/Hex.hpp>

#include "Util.hpp"

namespace SSLxx {
    namespace {
        inline void updateDigest(EVP_MD_CTX* sCtx, std::string_view aInput)
        {
            if (1 != EVP_DigestUpdate(sCtx, aInput.data(), aInput.size()))
                throw Error("EVP_DigestUpdate");
        }

        template <class T>
        typename std::enable_if<std::is_integral_v<T>, void>::type updateDigest(EVP_MD_CTX* sCtx, const T& aInput)
        {
            if (1 != EVP_DigestUpdate(sCtx, &aInput, sizeof(T)))
                throw Error("EVP_DigestUpdate");
        }
    } // namespace

    // aKind is a EVP_sha256() for example
    template <class... T>
    inline std::string Digest(const EVP_MD* aKind, T&&... aInput)
    {
        unsigned int sLen = EVP_MD_size(aKind);
        std::string  sResult(sLen, '\0');
        auto         sCtx = makeDigestCtx();

        if (1 != EVP_DigestInit_ex(sCtx.get(), aKind, NULL))
            throw Error("EVP_DigestInit_ex");

        Mpl::for_each_argument(
            [&](const auto& aInput) {
                updateDigest(sCtx.get(), aInput);
            },
            aInput...);

        if (1 != EVP_DigestFinal_ex(sCtx.get(), (uint8_t*)sResult.data(), &sLen))
            throw Error("EVP_DigestFinal_ex");

        assert(sLen == sResult.size());

        return sResult;
    }

    template <class... T>
    inline std::string DigestStr(const EVP_MD* aKind, T&&... aInput)
    {
        return Format::to_hex(Digest(aKind, std::forward<T&&>(aInput)...));
    }

    template <class... T>
    inline uint64_t DigestHash(const EVP_MD* aKind, T&&... aInput)
    {
        const auto  sData = Digest(aKind, std::forward<T&&>(aInput)...);
        const char* sPtr  = sData.data() + sData.size() - sizeof(uint64_t);
        return *reinterpret_cast<const uint64_t*>(sPtr);
    }

    template <class... T>
    inline bool DigestNth(const EVP_MD* aKind, uint64_t aPart, T&&... aInput)
    {
        return DigestHash(aKind, std::forward<T&&>(aInput)...) % aPart == 0;
    }

    inline std::string Scrypt(const std::string_view aPass, const std::string& aSalt, size_t aSize)
    {
        std::string sResult(aSize, ' ');

        if (1 != EVP_PBE_scrypt(aPass.data(), aPass.size(),
                                (const uint8_t*)aSalt.data(), aSalt.size(),
                                16384 /* N */, 8 /* r */ , 1 /* p */, 0 /*  max_mem */,
                                (uint8_t*)sResult.data(), sResult.size()))
            Error("EVP_PBE_scrypt");

        return sResult;
    }
} // namespace SSLxx
