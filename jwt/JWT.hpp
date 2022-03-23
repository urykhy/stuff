#pragma once

#include <string_view>

#include <exception/Error.hpp>
#include <format/Base64.hpp>
#include <format/Hex.hpp>
#include <format/Json.hpp>
#include <parser/Base64.hpp>
#include <parser/Json.hpp>
#include <parser/Parser.hpp>
#include <ssl/HMAC.hpp>

namespace Jwt {

    struct Header
    {
        std::string typ = "JWT";
        std::string alg = {};

        void from_json(const ::Json::Value& aJson)
        {
            Parser::Json::from_object(aJson, "typ", typ);
            Parser::Json::from_object(aJson, "alg", alg);
        }
        Format::Json::Value to_json() const
        {
            Format::Json::Value sValue(::Json::objectValue);
            sValue["typ"] = Format::Json::to_value(typ);
            sValue["alg"] = Format::Json::to_value(alg);
            return sValue;
        }
    };

    struct Claim
    {
        time_t      exp = {}; // expiration time on and after which the JWT must not be accepted for processing.
        time_t      nbf = {}; // time on which the JWT will start to be accepted for processing.
        std::string iss = {}; // issuer
        std::string aud = {}; // audience
        std::string sub = {}; // subject

        void from_json(const ::Json::Value& aJson)
        {
            Parser::Json::from_object(aJson, "exp", exp);
            Parser::Json::from_object(aJson, "nbf", nbf);
            Parser::Json::from_object(aJson, "iss", iss);
            Parser::Json::from_object(aJson, "aud", aud);
            Parser::Json::from_object(aJson, "sub", sub);
        }
        Format::Json::Value to_json() const
        {
            Format::Json::Value sValue(::Json::objectValue);
            sValue["exp"] = Format::Json::to_value(exp);
            sValue["nbf"] = Format::Json::to_value(nbf);
            sValue["iss"] = Format::Json::to_value(iss);
            sValue["aud"] = Format::Json::to_value(aud);
            sValue["sub"] = Format::Json::to_value(sub);
            return sValue;
        }
    };

    using Error = std::invalid_argument;

    struct IFace
    {
        virtual std::string Sign(const Claim& aClaim) const               = 0;
        virtual Claim       Validate(const std::string_view aToken) const = 0;
        virtual ~IFace(){};
    };

    class HS256 : public IFace
    {
        const SSLxx::KeyPtr m_Key;
        static unsigned constexpr URL_SAFE = Format::BASE64_NO_PADDING | Format::BASE64_URL_SAFE;

    public:
        HS256(const std::string& aKey)
        : m_Key(SSLxx::hmacKey(aKey))
        {}

        virtual ~HS256() {}

        std::string Sign(const Claim& aClaim) const override
        {
            std::string sHeader = Format::Base64(Format::Json::to_string(Format::Json::to_value(Header{.alg = "HS256"}), false), URL_SAFE);
            std::string sClaim  = Format::Base64(Format::Json::to_string(Format::Json::to_value(aClaim), false), URL_SAFE);
            std::string sHash   = SSLxx::HMAC::Sign(EVP_sha256(), m_Key, sHeader, std::string_view("."), sClaim);

            return sHeader + '.' + sClaim + '.' + Format::Base64(sHash, URL_SAFE);
        }

        Claim Validate(const std::string_view aToken) const override
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