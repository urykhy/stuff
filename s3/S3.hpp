#pragma once

#include <fmt/core.h>

#include <string>

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <rapidxml/rapidxml.hpp>
#include <ssl/Digest.hpp>
#include <ssl/HMAC.hpp>
#include <time/Time.hpp>

namespace S3 {
    struct Params
    {
        std::string host       = "127.0.0.1:9000";
        std::string bucket     = "test";
        std::string access_key = "minio";
        std::string secret_key = "minio123";
        std::string region     = "us-east-1";

        Curl::Client::Params curl;
    };

    class API
    {
        const Params m_Params;
        Time::Zone   m_Zone;

        std::string authorization(
            std::string_view aMethod,
            std::string_view aName,
            std::string_view aHash,
            std::string_view aDateTime) const
        {
            using namespace SSLxx;
            using namespace SSLxx::HMAC;

            const std::string_view sDate = aDateTime.substr(0, 8);
            const std::string      sCR   = fmt::format(
                "{}\n"      // HTTP method
                "/{}/{}\n"  // URI
                "\n"        // query string
                "host:{}\n" // headers
                "x-amz-content-sha256:{}\n"
                "x-amz-date:{}\n"
                "\n"
                "host;x-amz-content-sha256;x-amz-date\n" // signed headers
                "{}",                                    // content hash
                aMethod, m_Params.bucket, aName, m_Params.host, aHash, aDateTime, aHash);

            const std::string sSTS = fmt::format(
                "AWS4-HMAC-SHA256\n"
                "{}\n"
                "{}/{}/s3/aws4_request\n"
                "{}",
                aDateTime, sDate, m_Params.region, DigestStr(EVP_sha256(), sCR));

            const std::string sDateKey    = Sign(EVP_sha256(), hmacKey("AWS4" + m_Params.secret_key), sDate);
            const std::string sRegionKey  = Sign(EVP_sha256(), hmacKey(sDateKey), m_Params.region);
            const std::string sServiceKey = Sign(EVP_sha256(), hmacKey(sRegionKey), std::string_view("s3"));
            const std::string sSigningKey = Sign(EVP_sha256(), hmacKey(sServiceKey), std::string_view("aws4_request"));
            const std::string sSignature  = Format::to_hex(Sign(EVP_sha256(), hmacKey(sSigningKey), sSTS));

            std::string sHeader = fmt::format(
                "AWS4-HMAC-SHA256 "
                "Credential={}/{}/{}/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature={}",
                m_Params.access_key, sDate, m_Params.region, sSignature);

            return sHeader;
        }

        Curl::Client::Params prepare(std::string_view aMethod, std::string_view& aName, std::string_view aContentHash) const
        {
            const time_t      sNow      = ::time(nullptr);
            const std::string sDateTime = m_Zone.format(sNow, Time::ISO8601);

            Curl::Client::Params sParams            = m_Params.curl;
            sParams.headers["Authorization"]        = authorization(aMethod, aName, aContentHash, sDateTime);
            sParams.headers["x-amz-content-sha256"] = aContentHash;
            sParams.headers["x-amz-date"]           = sDateTime;
            return sParams;
        }

        std::string location(std::string_view aName) const
        {
            return fmt::format("http://{}/{}/{}", m_Params.host, m_Params.bucket, aName);
        }

        void reportError(Curl::Client::Result& aResult) const
        {
            std::string sMsg;

            auto sIt = aResult.headers.find("Content-Type");
            if (sIt != aResult.headers.end() and sIt->second == "application/xml") {
                try {
                    rapidxml::xml_document<> sDoc;
                    sDoc.parse<rapidxml::parse_non_destructive>(aResult.body.data());
                    auto sNode = sDoc.first_node("Error");
                    if (sNode) {
                        sNode = sNode->first_node("Message");
                        if (sNode)
                            sMsg.assign(sNode->value(), sNode->value_size());
                    }
                } catch (...) {
                }
            }
            if (sMsg.empty())
                sMsg = std::move(aResult.body);
            throw Error(sMsg, aResult.status);
        }

    public:
        API(const Params& aParams)
        : m_Params(aParams)
        , m_Zone(Time::load("UTC"))
        {}

        using Error = Exception::HttpError;

        void PUT(std::string_view aName, std::string_view aContent) const
        {
            const std::string sContentHash = SSLxx::DigestStr(EVP_sha256(), aContent);
            Curl::Client      sClient(prepare("PUT", aName, sContentHash));
            auto              sResult = sClient.PUT(location(aName), aContent);
            if (sResult.status != 200)
                reportError(sResult);
        }

        std::string GET(std::string_view aName) const
        {
            const std::string sContentHash = SSLxx::DigestStr(EVP_sha256(), std::string_view{});
            Curl::Client      sClient(prepare("GET", aName, sContentHash));
            auto              sResult = sClient.GET(location(aName));
            if (sResult.status != 200)
                reportError(sResult);
            return sResult.body;
        }

        void DELETE(std::string_view aName) const
        {
            const std::string sContentHash = SSLxx::DigestStr(EVP_sha256(), std::string_view{});
            Curl::Client      sClient(prepare("DELETE", aName, sContentHash));
            auto              sResult = sClient.DELETE(location(aName));
            if (sResult.status != 204)
                reportError(sResult);
        }
    };

} // namespace S3