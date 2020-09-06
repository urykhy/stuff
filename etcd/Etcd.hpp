#pragma once

#include <json/json.h>

#include <curl/Curl.hpp>
#include <file/Util.hpp>
#include <format/Base64.hpp>
#include <parser/Atoi.hpp>
#include <parser/Base64.hpp>

#ifndef BOOST_TEST_MESSAGE
#define BOOST_TEST_MESSAGE(x)
#endif

namespace Etcd {
    struct Client
    {
        struct Params
        {
            std::string url    = "http://127.0.0.1:2379";
            std::string prefix = "test:";
        };

        using Error = std::runtime_error;

        struct Pair
        {
            std::string key;
            std::string value;
        };
        using List = std::list<Pair>;

    private:
        const Params         m_Params;
        Curl::Client::Params m_CurlParams;
        Curl::Client         m_Client;

        Pair decodePair(const Json::Value& aNode)
        {
            std::string sKey = Parser::Base64(aNode["key"].asString());
            sKey.erase(0, m_Params.prefix.size());
            return {sKey, Parser::Base64(aNode["value"].asString())};
        }

        Json::Value request(const std::string& aAPI, const Json::Value& aBody)
        {
            Json::StreamWriterBuilder sBuilder;
            auto                      sBody = Json::writeString(sBuilder, aBody);
            //BOOST_TEST_MESSAGE("etcd <- " << aAPI << ' ' << sBody);

            auto&& [sCode, sResult] = m_Client.POST(m_Params.url + "/v3/" + aAPI, sBody);
            //BOOST_TEST_MESSAGE("etcd -> " << sResult);
            if (sCode == 200) {
                Json::Value  sJson;
                Json::Reader sReader;
                if (!sReader.parse(sResult, sJson))
                    throw Error("etcd: bad server response: " + sReader.getFormattedErrorMessages());
                return sJson;
            }
            if (sCode == 404)
                return Json::Value();
            throw Error("etcd: http code: " + std::to_string(sCode) + ", message: " + sResult);
        }

    public:
        Client(const Params& aParams)
        : m_Params(aParams)
        , m_Client(m_CurlParams)
        {}

        std::string get(const std::string& aKey)
        {
            Json::Value sRoot;
            sRoot["key"]       = Format::Base64(m_Params.prefix + aKey);
            const auto sResult = request("kv/range", sRoot);

            if (sResult.isObject())
                return decodePair(sResult["kvs"][0]).value;
            return "";
        }

        void put(const std::string& aKey, const std::string& aValue, int64_t aLease = 0)
        {
            Json::Value sRoot;
            sRoot["key"]   = Format::Base64(m_Params.prefix + aKey);
            sRoot["value"] = Format::Base64(aValue);
            if (aLease > 0)
                sRoot["lease"] = Json::Value::Int64(aLease);

            request("kv/put", sRoot);
        }

        void remove(const std::string& aKey)
        {
            Json::Value sRoot;
            sRoot["key"] = Format::Base64(m_Params.prefix + aKey);
            request("kv/deleterange", sRoot);
        }

        List list(const std::string& aKey, int64_t aLimit = 0)
        {
            Json::Value sRoot;
            auto        sKey = m_Params.prefix + aKey;
            sRoot["key"]     = Format::Base64(sKey);
            sKey.back()++;
            sRoot["range_end"] = Format::Base64(sKey);
            if (aLimit > 0)
                sRoot["limit"] = Json::Value::Int64(aLimit);

            const auto sResult = request("kv/range", sRoot);

            List sList;
            if (sResult.isObject()) {
                auto&& sKvs = sResult["kvs"];
                for (Json::Value::ArrayIndex i = 0; i != sKvs.size(); i++)
                    sList.emplace_back(decodePair(sKvs[i]));

                return sList;
            }
            return sList;
        }

        // lease api
        int64_t createLease(int32_t aTTL)
        {
            Json::Value sRoot;
            sRoot["TTL"]       = Json::Value::Int64(aTTL);
            const auto sResult = request("lease/grant", sRoot);
            return Parser::Atoi<int64_t>(sResult["ID"].asString());
        }

        void updateLease(int64_t aLease)
        {
            Json::Value sRoot;
            sRoot["ID"] = Json::Value::Int64(aLease);
            request("lease/keepalive", sRoot);
        }

        void dropLease(int64_t aLease)
        {
            Json::Value sRoot;
            sRoot["ID"] = Json::Value::Int64(aLease);
            request("lease/revoke", sRoot);
        }
    }; // namespace Etcd
} // namespace Etcd