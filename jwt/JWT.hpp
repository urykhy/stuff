#pragma once

#include <exception/Error.hpp>
#include <format/Base64.hpp>
#include <format/Hex.hpp>
#include <format/Json.hpp>
#include <parser/Base64.hpp>
#include <parser/Json.hpp>
#include <parser/Parser.hpp>
#include <ssl/HMAC.hpp>

namespace Jwt {

    struct Claim
    {
        time_t      exp      = {}; // expiration time on and after which the JWT must not be accepted for processing.
        time_t      nbf      = {}; // time on which the JWT will start to be accepted for processing.
        std::string iss      = {}; // issuer
        std::string aud      = {}; // audience
        std::string username = {};

        void from_json(const ::Json::Value& aJson)
        {
            Parser::Json::from_value(aJson, "exp", exp);
            Parser::Json::from_value(aJson, "nbf", nbf);
            Parser::Json::from_value(aJson, "iss", iss);
            Parser::Json::from_value(aJson, "aud", aud);
            Parser::Json::from_value(aJson, "username", username);
        }
        Format::Json::Value to_json() const
        {
            Format::Json::Value sValue(::Json::objectValue);
            sValue["exp"]      = Format::Json::to_value(exp);
            sValue["nbf"]      = Format::Json::to_value(nbf);
            sValue["iss"]      = Format::Json::to_value(iss);
            sValue["aud"]      = Format::Json::to_value(aud);
            sValue["username"] = Format::Json::to_value(username);
            return sValue;
        }
    };

    class Manager
    {
        const SSLxx::KeyPtr m_Key;
        static unsigned constexpr URL_SAFE = Format::BASE64_NO_PADDING | Format::BASE64_URL_SAFE;

    public:
        struct Header
        {
            std::string typ = "JWT";
            std::string alg = "HS256";

            void from_json(const ::Json::Value& aJson)
            {
                Parser::Json::from_value(aJson, "typ", typ);
                Parser::Json::from_value(aJson, "alg", alg);
            }
            Format::Json::Value to_json() const
            {
                Format::Json::Value sValue(::Json::objectValue);
                sValue["typ"] = Format::Json::to_value(typ);
                sValue["alg"] = Format::Json::to_value(alg);
                return sValue;
            }
        };

        using Error = Exception::Error<Manager, std::invalid_argument>;

        Manager(const std::string& aKey)
        : m_Key(SSLxx::hmacKey(aKey))
        {}

        std::string Sign(const Claim& aClaim) const
        {
            std::string sHeader = Format::Base64(Format::Json::to_string(Format::Json::to_value(Header{}), false), URL_SAFE);
            std::string sClaim  = Format::Base64(Format::Json::to_string(Format::Json::to_value(aClaim), false), URL_SAFE);
            std::string sHash   = SSLxx::HMAC::Sign(EVP_sha256(), m_Key, sHeader, std::string_view("."), sClaim);

            return sHeader + '.' + sClaim + '.' + Format::Base64(sHash, URL_SAFE);
        }

        Claim Validate(const std::string& aToken) const
        {
            std::vector<std::string_view> sParts;
            Parser::simple(
                aToken,
                [&sParts](auto sPart) mutable {
                    sParts.push_back(sPart);
                },
                '.');

            const std::string sHash = Parser::Base64(sParts[2], URL_SAFE);

            if (!SSLxx::HMAC::Verify(EVP_sha256(),
                                     m_Key,
                                     sHash,
                                     sParts[0], std::string_view("."), sParts[1]))
                throw Error("invalid");

            Header sHeader;
            Parser::Json::from_value(Parser::Json::parse(Parser::Base64(sParts[0], URL_SAFE)), sHeader);
            if (sHeader.typ != "JWT")
                throw Error("unexpected type: " + sHeader.typ);
            if (sHeader.alg != "HS256")
                throw Error("not supported algorithm: " + sHeader.alg);

            Claim sClaim;
            Parser::Json::from_value(Parser::Json::parse(Parser::Base64(sParts[1], URL_SAFE)), sClaim);
            const time_t sNow = time(nullptr);
            if (sNow > sClaim.exp or sNow < sClaim.nbf)
                throw Error("expired");

            return sClaim;
        }
    };

} // namespace Jwt