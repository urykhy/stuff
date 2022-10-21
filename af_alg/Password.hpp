#pragma once

#include <unsorted/Random.hpp>
#include "Digest.hpp"

namespace af_alg
{
    struct PasswordConfig
    {
        std::string hash;
        std::string password;
        size_t size;
    };

    class Password
    {
        DigestImpl m_Digest;
    public:

        static const size_t SALT_LEN = 8;

        Password(const PasswordConfig& c)
        : m_Digest(c.hash, c.size, c.password)
        { }

        std::string create(const std::string& aPass)
        {
            const std::string sSalt = Util::randomStr(SALT_LEN);
            const std::string sHash = Digest(m_Digest, aPass, sSalt);
            return sHash + sSalt;
        }

        bool validate(const std::string& aPass, const std::string& aHash)
        {
            if (aHash.size() < SALT_LEN)
                return false;
            const std::string sSalt = aHash.substr(aHash.size() - SALT_LEN);
            const std::string sHash = Digest(m_Digest, aPass, sSalt);
            return sHash + sSalt == aHash;
        }
    };
}
