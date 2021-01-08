#pragma once

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>
#include <stdexcept>
#include <string_view>

#include <mpl/Mpl.hpp>
#include <unsorted/Raii.hpp>

namespace SSLxx {

    using Error = std::runtime_error;

    using KeyPtr = std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)>;

    template <auto K>
    KeyPtr readKey(std::string_view aStr)
    {
        BIO* sIO = BIO_new_mem_buf(aStr.data(), aStr.size());
        if (sIO == nullptr)
            throw std::runtime_error("Bio_new");
        Util::Raii sCleanup([&sIO]() { BIO_free(sIO); });

        EVP_PKEY* key = K(sIO, 0, 0, 0);
        if (key == nullptr)
            throw std::invalid_argument("readKey");
        return KeyPtr(key, EVP_PKEY_free);
    };

    KeyPtr publicKey(std::string_view aStr) { return readKey<PEM_read_bio_PUBKEY>(aStr); }
    KeyPtr privateKey(std::string_view aStr) { return readKey<PEM_read_bio_PrivateKey>(aStr); }

    KeyPtr hmacKey(const std::string_view aKey)
    {
        auto* sKey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, (const uint8_t*)aKey.data(), aKey.size());
        if (sKey == nullptr)
            throw std::runtime_error("EVP_PKEY_new_mac_key");
        return KeyPtr(sKey, EVP_PKEY_free);
    }

    using DigestCtx = std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX*)>;
    DigestCtx makeDigestCtx()
    {
        auto* sCtx = EVP_MD_CTX_create();
        if (sCtx == nullptr)
            throw std::runtime_error("EVP_MD_CTX_create");
        return DigestCtx(sCtx, EVP_MD_CTX_free);
    }

    using CipherCtx = std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX*)>;
    CipherCtx makeCipherCtx()
    {
        auto* sCtx = EVP_CIPHER_CTX_new();
        if (sCtx == nullptr)
            throw std::runtime_error("EVP_CIPHER_CTX_new");
        return CipherCtx(sCtx, EVP_CIPHER_CTX_free);
    }

} // namespace SSLxx
