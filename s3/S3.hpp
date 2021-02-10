#pragma once

#include <fmt/core.h>

#include <string>

#include <curl/Curl.hpp>
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
    };

    inline std::string Authorization(const Params& aParams, const std::string_view aMethod, const std::string& aName, const std::string aHash, std::string_view aDateTime)
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
            aMethod, aParams.bucket, aName, aParams.host, aHash, aDateTime, aHash);

        const std::string sSTS = fmt::format(
            "AWS4-HMAC-SHA256\n"
            "{}\n"
            "{}/{}/s3/aws4_request\n"
            "{}",
            aDateTime, sDate, aParams.region, DigestStr(EVP_sha256(), sCR));

        const std::string sDateKey    = Sign(EVP_sha256(), hmacKey("AWS4" + aParams.secret_key), sDate);
        const std::string sRegionKey  = Sign(EVP_sha256(), hmacKey(sDateKey), aParams.region);
        const std::string sServiceKey = Sign(EVP_sha256(), hmacKey(sRegionKey), std::string_view("s3"));
        const std::string sSigningKey = Sign(EVP_sha256(), hmacKey(sServiceKey), std::string_view("aws4_request"));
        const std::string sSignature  = Format::to_hex(Sign(EVP_sha256(), hmacKey(sSigningKey), sSTS));

        std::string sHeader = fmt::format(
            "AWS4-HMAC-SHA256 "
            "Credential={}/{}/{}/s3/aws4_request,"
            "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
            "Signature={}",
            aParams.access_key, sDate, aParams.region, sSignature);

        return sHeader;
    }

    inline auto PUT(const Params& aParams, const std::string& aName, const std::string& aContent)
    {
        const time_t      sNow = ::time(nullptr);
        Time::Zone        sZone(Time::load("UTC"));
        const std::string sDateTime    = sZone.format(sNow, Time::ISO8601);
        const std::string sContentHash = SSLxx::DigestStr(EVP_sha256(), aContent);

        Curl::Client::Params sParams;
        sParams.headers.push_back({"Authorization", Authorization(aParams, "PUT", aName, sContentHash, sDateTime)});
        sParams.headers.push_back({"x-amz-content-sha256", sContentHash});
        sParams.headers.push_back({"x-amz-date", sDateTime});

        Curl::Client sClient(sParams);
        return sClient.PUT("http://" + aParams.host + "/" + aParams.bucket + "/" + aName, aContent);
    }
} // namespace S3