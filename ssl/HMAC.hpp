#pragma once

#include <openssl/evp.h>

#include <cassert>
#include <string>
#include <string_view>
#include <mpl/Mpl.hpp>
#include <unsorted/Raii.hpp>

namespace SSLxx::HMAC
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
    template<class... T>
    inline std::string Sign(const EVP_MD* aKind, const Key& aKey, T&&... aInput)
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
        Mpl::for_each_argument([&](const auto& aInput){
            if (1 != EVP_DigestSignUpdate(sCtx, aInput.data(), aInput.size())) throw Error("EVP_DigestSignUpdate");
        }, aInput...);
        if (1 != EVP_DigestSignFinal(sCtx, (uint8_t*)sResult.data(), &sLen)) throw Error("EVP_DigestSignFinal");

        assert(sLen == sResult.size());

        return sResult;
    }

    template<class... T>
    bool Verify(const EVP_MD* aKind, const Key& aKey, const std::string_view aHash, T&&... aInput)
    {
        const std::string sHash = Sign(aKind, aKey, std::forward<T&&>(aInput)...);
        assert(sHash.size() == aHash.size());
        return 0 == CRYPTO_memcmp(sHash.data(), aHash.data(), aHash.size());
    }
} // HMAC
