#pragma once

#include <fmt/core.h>

#include <string>

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <format/Base64.hpp>
#include <format/XML.hpp>
#include <parser/AWS.hpp>
#include <parser/Hex.hpp>
#include <parser/XML.hpp>
#include <ssl/Digest.hpp>
#include <ssl/HMAC.hpp>
#include <string/String.hpp>
#include <time/Time.hpp>
#include <unsorted/Env.hpp>
#include <unsorted/Raii.hpp>

namespace S3 {
    struct Params
    {
        std::string host       = Util::getEnv("S3_HOST");
        std::string bucket     = Util::getEnv("S3_BUCKET");
        std::string access_key = Util::getEnv("S3_ACCESS_KEY");
        std::string secret_key = Util::getEnv("S3_SECRET_KEY");
        std::string region     = Util::getEnv("S3_REGION");

        Curl::Client::Default curl;
    };

    class API
    {
        const Params m_Params;
        Time::Zone   m_Zone;
        Curl::Client m_Client;

        std::string authorization(
            Curl::Client::Method         aMethod,
            const Curl::Client::Headers& aHeaders,
            std::string_view             aName,
            std::string_view             aQuery,
            std::string_view             aHash,
            std::string_view             aDateTime) const
        {
            using namespace SSLxx;
            using namespace SSLxx::HMAC;

            const std::string_view sDate = aDateTime.substr(0, 8);

            std::string_view sMethod;
            switch (aMethod) {
            case Curl::Client::Method::DELETE: sMethod = "DELETE"; break;
            case Curl::Client::Method::GET: sMethod = "GET"; break;
            case Curl::Client::Method::HEAD: sMethod = "HEAD"; break;
            case Curl::Client::Method::POST: sMethod = "POST"; break;
            case Curl::Client::Method::PUT: sMethod = "PUT"; break;
            default: throw std::invalid_argument("unknown method");
            }

            // sign 'host' and all amz headers. in order.
            std::string sHeaders       = fmt::format("host:{}\n", m_Params.host);
            std::string sSignedHeaders = "host";

            for (const auto& [sName, sValue] : aHeaders) {
                if (String::starts_with(sName, "x-amz-")) {
                    sHeaders += fmt::format("{}:{}\n", sName, sValue);
                    sSignedHeaders += fmt::format(";{}", sName);
                }
            }

            const std::string sCR = fmt::format(
                "{}\n"     // HTTP method
                "/{}/{}\n" // URI
                "{}\n"     // query
                "{}\n"     // headers
                "{}\n"     // signed headers
                "{}",      // content hash
                sMethod, m_Params.bucket, aName, aQuery, sHeaders, sSignedHeaders, aHash);

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
                "SignedHeaders={},"
                "Signature={}",
                m_Params.access_key, sDate, m_Params.region, sSignedHeaders, sSignature);

            return sHeader;
        }

        using ContentWithHash = std::pair<std::string_view, std::string_view>;

        Curl::Client::Request make(Curl::Client::Method aMethod,
                                   std::string_view     aName,
                                   ContentWithHash      aContentWithHash = {},
                                   std::string_view     aQuery           = {}) const
        {
            const auto [aContent, aContentHash] = aContentWithHash;
            const time_t      sNow              = ::time(nullptr);
            const std::string sDateTime         = m_Zone.format(sNow, Time::ISO8601_TZ);
            const std::string sContentHash(aContentHash.empty() ? SSLxx::DigestStr(EVP_sha256(), aContent) : aContentHash);

            Curl::Client::Request sRequest;

            sRequest.method = aMethod;
            sRequest.url    = fmt::format("http://{}/{}/{}?{}", m_Params.host, m_Params.bucket, aName, aQuery);
            sRequest.body   = aContent;
            // BOOST_TEST_MESSAGE("url: " << sRequest.url);

            sRequest.headers["x-amz-content-sha256"] = sContentHash;
            sRequest.headers["x-amz-date"]           = sDateTime;
            if (aMethod == Curl::Client::Method::PUT or aMethod == Curl::Client::Method::POST)
                sRequest.headers["x-amz-meta-sha256"] = sContentHash;
            if (aMethod == Curl::Client::Method::POST and aQuery == "uploads=") // create multipart upload
                sRequest.headers["x-amz-checksum-algorithm"] = "SHA256";
            if (aMethod == Curl::Client::Method::PUT and aQuery.starts_with("partNumber=")) // upload part
                sRequest.headers["x-amz-checksum-sha256"] = Format::Base64(Parser::from_hex(sContentHash));
            sRequest.headers["Authorization"] = authorization(aMethod, sRequest.headers, aName, aQuery, sContentHash, sDateTime);

            return m_Params.curl.wrap(std::move(sRequest));
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

        auto parseXML(Curl::Client::Result& aResult)
        {
            auto sHeader = aResult.headers.find("Content-Type");
            if (sHeader == aResult.headers.end() or sHeader->second != "application/xml")
                throw std::runtime_error("XML response expected");
            return Parser::XML::parse(aResult.body);
        }

        std::string startMultipartUpload(std::string_view aName, std::string_view aSha256)
        {
            auto sResult = m_Client(make(Curl::Client::Method::POST, aName, {"", aSha256}, "uploads="));
            if (sResult.status != 200)
                reportError(sResult);
            auto        sXML = Parser::XML::parse(sResult.body);
            std::string sUploadId;
            Parser::XML::from_path(sXML.get(), "InitiateMultipartUploadResult.UploadId", sUploadId);
            if (sUploadId.empty())
                throw std::runtime_error("UploadId not found");
            return sUploadId;
        }

        struct UploadTag
        {
            std::string etag;
            std::string hash;
        };
        UploadTag uploadPart(std::string_view aName, const std::string& aUploadId,
                             uint32_t aNumber, std::string_view aContent)
        {
            const std::string sHash = SSLxx::DigestStr(EVP_sha256(), aContent);

            auto sResult = m_Client(make(Curl::Client::Method::PUT, aName,
                                         {aContent, sHash},
                                         fmt::format("partNumber={}&uploadId={}", aNumber, aUploadId)));
            if (sResult.status != 200)
                reportError(sResult);
            return {sResult.headers["ETag"], sHash};
        }

        void completeMultipartUpload(std::string_view aName, const std::string& aUploadId,
                                     const std::vector<UploadTag>& aETags)
        {
            std::string sBody;
            {
                auto sXML    = std::make_unique<rapidxml::xml_document<>>();
                auto sUpload = format::XML::create_object(sXML.get(), "CompleteMultipartUpload");

                for (uint32_t aId = 0; aId < aETags.size(); aId++) {
                    auto sPart = format::XML::create_object(sUpload, "Part");
                    format::XML::write(sPart, "PartNumber", std::to_string(aId + 1));
                    format::XML::write(sPart, "ETag", aETags.at(aId).etag);
                    format::XML::write(sPart, "ChecksumSHA256", Format::Base64(Parser::from_hex(aETags.at(aId).hash)));
                }
                sBody = format::XML::to_string(sXML.get());
            }

            auto sResult = m_Client(make(Curl::Client::Method::POST, aName, {sBody, ""}, "uploadId=" + aUploadId));
            if (sResult.status != 200)
                reportError(sResult);

            // check if we really got successful response
            {
                auto        sXML = parseXML(sResult);
                std::string sMsg;
                Parser::XML::from_path(sXML.get(), "Error.Message", sMsg);
                if (!sMsg.empty()) {
                    throw Error(sMsg, sResult.status);
                }
                std::string sServerHash;
                Parser::XML::from_path(sXML.get(), "CompleteMultipartUploadResult.ChecksumSHA256", sServerHash);
                if (sServerHash.empty())
                    throw std::runtime_error("ChecksumSHA256 not found");
            }
        }

    public:
        API(const Params& aParams)
        : m_Params(aParams)
        , m_Zone(Time::load("UTC"))
        {
        }

        using Error = Exception::HttpError;

        void PUT(std::string_view aName, std::string_view aContent, std::string_view aSha256 = {})
        {
            auto sResult = m_Client(make(Curl::Client::Method::PUT, aName, {aContent, aSha256}));
            if (sResult.status != 200)
                reportError(sResult);
        }

        std::string GET(std::string_view aName, int aPart = 0)
        {
            std::string sPart;
            if (aPart > 0)
                sPart = "partNumber=" + std::to_string(aPart);
            auto sResult = m_Client(make(Curl::Client::Method::GET, aName, {}, sPart));
            if (sResult.status != 200 and sResult.status != 206)
                reportError(sResult);
            if (auto sIt = sResult.headers.find("x-amz-meta-sha256"); sIt != sResult.headers.end() and aPart == 0) {
                if (SSLxx::DigestStr(EVP_sha256(), sResult.body) != sIt->second)
                    throw std::runtime_error("Checksum mismatch");
            }
            return sResult.body;
        }

        struct HeadResult
        {
            int         status = 0;
            int         parts  = 0;
            size_t      size   = 0;
            time_t      mtime  = 0;
            std::string sha256;
        };

        HeadResult HEAD(std::string_view aName, int aPart = 0)
        {
            HeadResult  sHead;
            std::string sPart;
            if (aPart > 0)
                sPart = "partNumber=" + std::to_string(aPart);
            auto sResult = m_Client(make(Curl::Client::Method::HEAD, aName, {}, sPart));
            sHead.status = sResult.status;
            if (sHead.status != 200 and sHead.status != 206)
                return sHead;
            if (auto sIt = sResult.headers.find("Content-Length"); sIt != sResult.headers.end())
                sHead.size = Parser::Atoi<size_t>(sIt->second);
            if (auto sIt = sResult.headers.find("Last-Modified"); sIt != sResult.headers.end())
                sHead.mtime = m_Zone.parse(sIt->second, Time::RFC1123);
            if (auto sIt = sResult.headers.find("x-amz-meta-sha256"); sIt != sResult.headers.end())
                sHead.sha256 = sIt->second;
            if (auto sIt = sResult.headers.find("ETag"); sIt != sResult.headers.end()) {
                std::string_view sTmp = sIt->second;
                sTmp.remove_suffix(1); // terminating "
                if (auto sPos = sTmp.find_last_of('-'); sPos != std::string_view::npos)
                    sHead.parts = Parser::Atoi<size_t>(sTmp.substr(sPos + 1));
            }
            return sHead;
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

            try {
                Util::Raii sCleanZonePtr([this]() { m_ZonePtr = nullptr; });
                m_ZonePtr = &m_Zone;

                auto sXML = parseXML(sResult);
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

        void multipartPUT(std::string_view aName, std::function<std::string()> aSrc, std::string_view aSha256 = {})
        {
            std::string sUploadId = startMultipartUpload(aName, aSha256);
            uint32_t    sSerial   = 1;

            std::vector<UploadTag> sETags;
            while (true) {
                auto sTmp = aSrc();
                if (sTmp.empty())
                    break;
                sETags.push_back(uploadPart(aName, sUploadId, sSerial, sTmp));
                sSerial++;
            }
            completeMultipartUpload(aName, sUploadId, sETags);
        }

        std::string SELECT(std::string_view aName, const std::string& aQuery)
        {
            auto sXML     = std::make_unique<rapidxml::xml_document<>>();
            auto sRequest = format::XML::create_object(sXML.get(), "SelectRequest");
            format::XML::write(sRequest, "Expression", aQuery);
            format::XML::write(sRequest, "ExpressionType", "SQL");
            auto sInput = format::XML::create_object(sRequest, "InputSerialization");
            // format::XML::write(sInput, "CompressionType", "GZIP");
            auto sCSV = format::XML::create_object(sInput, "CSV");
            format::XML::write(sCSV, "FileHeaderInfo", "Use");
            format::XML::write(sCSV, "RecordDelimiter", "\n");
            format::XML::write(sCSV, "FieldDelimiter", ",");
            format::XML::write(sCSV, "QuoteCharacter", "\"");
            format::XML::write(sCSV, "QuoteEscapeCharacter", "\"");
            format::XML::write(sCSV, "Comments", "#");
            auto sOutput = format::XML::create_object(sRequest, "OutputSerialization");
            auto sOCSV   = format::XML::create_object(sOutput, "CSV");
            format::XML::write(sOCSV, "QuoteFields", "ASNEEDED");
            format::XML::write(sOCSV, "RecordDelimiter", "\n");
            format::XML::write(sOCSV, "FieldDelimiter", ",");
            format::XML::write(sOCSV, "QuoteCharacter", "\"");
            format::XML::write(sOCSV, "QuoteEscapeCharacter", "\"");
            std::string sBody = format::XML::to_string(sXML.get());

            auto sResult = m_Client(make(Curl::Client::Method::POST, aName, {sBody, ""}, "select=&select-type=2"));
            if (sResult.status != 200 and sResult.status != 206)
                reportError(sResult);

            // parsing response
            return Parser::AWS::Stream(sResult.body);
        }

    private:
        friend struct Entry;

        static thread_local Time::Zone* m_ZonePtr;

        static time_t parse(const std::string& aTime)
        {
            return m_ZonePtr->parse(aTime);
        }
    };

    inline thread_local Time::Zone* API::m_ZonePtr = nullptr;

} // namespace S3