#pragma once

#include <openssl/bio.h>
#include <openssl/pem.h>

#include <memory>
#include <stdexcept>
#include <string_view>

#include <unsorted/Raii.hpp>

namespace SSLxx {

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

} // namespace SSLxx