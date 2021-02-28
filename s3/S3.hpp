#pragma once

#include <fmt/core.h>

#include <string>

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <parser/XML.hpp>
#include <ssl/Digest.hpp>
#include <ssl/HMAC.hpp>
#include <time/Time.hpp>
#include <unsorted/Raii.hpp>

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
        Curl::Client m_Client;

        std::string authorization(
            std::string_view aMethod,
            std::string_view aName,
            std::string_view aQuery,
            std::string_view aHash,
            std::string_view aDateTime) const
        {
            using namespace SSLxx;
            using namespace SSLxx::HMAC;

            const std::string_view sDate = aDateTime.substr(0, 8);
            const std::string      sCR   = fmt::format(
                "{}\n"      // HTTP method
                "/{}/{}\n"  // URI
                "{}\n"      // query string
                "host:{}\n" // headers
                "x-amz-content-sha256:{}\n"
                "x-amz-date:{}\n"
                "\n"
                "host;x-amz-content-sha256;x-amz-date\n" // signed headers
                "{}",                                    // content hash
                aMethod, m_Params.bucket, aName, aQuery, m_Params.host, aHash, aDateTime, aHash);

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

        Curl::Client::Request make(Curl::Client::Method aMethod,
                                   std::string_view     aName,
                                   std::string_view     aContent = {},
                                   std::string_view     aQuery   = {}) const
        {
            const time_t      sNow         = ::time(nullptr);
            const std::string sDateTime    = m_Zone.format(sNow, Time::ISO8601_TZ);
            const std::string sContentHash = SSLxx::DigestStr(EVP_sha256(), aContent);
            std::string_view  sMethod;

            Curl::Client::Request sRequest;

            switch (aMethod) {
            case Curl::Client::Method::GET: sMethod = "GET"; break;
            case Curl::Client::Method::PUT: sMethod = "PUT"; break;
            case Curl::Client::Method::DELETE: sMethod = "DELETE"; break;
            default: throw std::invalid_argument("unknown method");
            }

            sRequest.method = aMethod;
            sRequest.url    = fmt::format("http://{}/{}/{}?{}", m_Params.host, m_Params.bucket, aName, aQuery);
            sRequest.body   = aContent;
            //BOOST_TEST_MESSAGE("url: " << sRequest.url);

            sRequest.headers["Authorization"]        = authorization(sMethod, aName, aQuery, sContentHash, sDateTime);
            sRequest.headers["x-amz-content-sha256"] = sContentHash;
            sRequest.headers["x-amz-date"]           = sDateTime;

            return sRequest;
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
                    if (sNode)
                        *sNode->first_node("Message") >> sMsg;
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
        , m_Client(aParams.curl)
        {}

        using Error = Exception::HttpError;

        void PUT(std::string_view aName, std::string_view aContent)
        {
            auto sResult = m_Client(make(Curl::Client::Method::PUT, aName, aContent));
            if (sResult.status != 200)
                reportError(sResult);
        }

        std::string GET(std::string_view aName)
        {
            auto sResult = m_Client(make(Curl::Client::Method::GET, aName));
            if (sResult.status != 200)
                reportError(sResult);
            return sResult.body;
        }

        void DELETE(std::string_view aName)
        {
            auto sResult = m_Client(make(Curl::Client::Method::DELETE, aName));
            if (sResult.status != 204)
                reportError(sResult);
        }

        struct Entry
        {
            std::string key;
            time_t      mtime = 0;
            size_t      size  = 0;

            void from_xml(const Parser::XML::Node* aNode)
            {
                if (aNode == nullptr)
                    return;
                std::string sLastModified;

                *aNode->first_node("Key") >> key;
                *aNode->first_node("LastModified") >> sLastModified;
                *aNode->first_node("Size") >> size;
                mtime = API::parse(sLastModified);
            }
        };

        struct List
        {
            bool truncated = false;

            std::vector<Entry> keys;
        };

        struct ListParams
        {
            std::string prefix = {};
            std::string after  = {};
            unsigned    limit  = {};
        };
        static ListParams defaultListParams() { return ListParams{}; }

        List LIST(const ListParams& aParams = defaultListParams())
        {
            List sList;

            // parameters must be in order
            std::string sQuery = "list-type=2";
            if (aParams.limit != 0)
                sQuery += ("&max-keys=" + std::to_string(aParams.limit));
            if (!aParams.prefix.empty())
                sQuery += ("&prefix=" + aParams.prefix);
            if (!aParams.after.empty())
                sQuery += ("&start-after=" + aParams.after);

            auto sResult = m_Client(make(Curl::Client::Method::GET, {}, {}, sQuery));
            if (sResult.status != 200)
                reportError(sResult);

            auto sHeader = sResult.headers.find("Content-Type");
            if (sHeader == sResult.headers.end() or sHeader->second != "application/xml")
                throw std::runtime_error("XML response expected");

            try {
                Util::Raii sCleanZonePtr([this]() { m_ZonePtr = nullptr; });
                m_ZonePtr = &m_Zone;

                auto sXML = Parser::XML::parse(sResult.body);
                auto sLBR = sXML->first_node("ListBucketResult");
                if (!sLBR)
                    throw std::runtime_error("ListBucketResult node not found");
                *sLBR->first_node("IsTruncated") >> sList.truncated;

                uint64_t sCount = 0;
                *sLBR->first_node("KeyCount") >> sCount;
                sList.keys.reserve(sCount);

                *sLBR->first_node("Contents") >> sList.keys;
                return sList;
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("XML processing error: ") + e.what());
            }
        }

    private:
        friend struct Entry;

        static thread_local Time::Zone* m_ZonePtr;

        static time_t parse(std::string& aTime)
        {
            auto sPos = aTime.find('.');
            if (sPos != std::string::npos) {
                aTime.erase(sPos);
                aTime.push_back('Z');
            }
            return m_ZonePtr->parse(aTime, Time::ISO8601_LTZ);
        }
    };

    inline thread_local Time::Zone* API::m_ZonePtr = nullptr;

} // namespace S3