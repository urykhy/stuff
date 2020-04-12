#pragma once

#include <openssl/evp.h>

#include <cassert>
#include <string>
#include <string_view>
#include <iostream>

#include <mpl/Mpl.hpp>
#include <unsorted/Raii.hpp>
#include <parser/Hex.hpp>

namespace SSLxx
{
    using Error = std::runtime_error;

    // aKind is a EVP_sha256() for example
    template<class... T>
    inline std::string Digest(const EVP_MD* aKind, T&&... aInput)
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
        Mpl::for_each_argument([&](const auto& aInput){
            if (1 != EVP_DigestUpdate(sCtx, aInput.data(), aInput.size())) throw Error("EVP_DigestUpdate");
        }, aInput...);
        if (1 != EVP_DigestFinal_ex(sCtx, (uint8_t*)sResult.data(), &sLen)) throw Error("EVP_DigestFinal_ex");

        assert(sLen == sResult.size());

        return sResult;
    }

    template<class... T>
    inline std::string DigestStr(const EVP_MD* aKind, T&&... aInput)
    {
        return Parser::to_hex(Digest(aKind, std::forward<T&&>(aInput)...));
    }

    template<class... T>
    inline uint64_t DigestHash(const EVP_MD* aKind, T&&... aInput)
    {
        const auto sData = Digest(aKind, std::forward<T&&>(aInput)...);
        const char* sPtr = sData.data() + sData.size() - sizeof(uint64_t);
        return *reinterpret_cast<const uint64_t*>(sPtr);
    }

    template<class... T>
    inline bool DigestNth(const EVP_MD* aKind, uint64_t aPart, T&&... aInput)
    {
        return DigestHash(aKind, std::forward<T&&>(aInput)...) % aPart == 0;
    }

    inline std::string Scrypt(const std::string_view aPass, const std::string& aSalt, size_t aSize)
    {
        std::string sResult(aSize, ' ');

        if (1 != EVP_PBE_scrypt(aPass.data(), aPass.size(),
                               (const uint8_t*)aSalt.data(), aSalt.size(),
                               1024, 8, 1, 0,
                               (uint8_t*)sResult.data(), sResult.size()))
            Error("EVP_PBE_scrypt");

        return sResult;
    }
}