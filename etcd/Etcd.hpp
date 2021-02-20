#pragma once

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <format/Hex.hpp>
#include <parser/Atoi.hpp>
#include <parser/Base64.hpp>
#include <parser/Json.hpp>

#include "Protocol.hpp"

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

        struct Pair
        {
            std::string key;
            std::string value;

            void from_json(const Format::Json::Value& aValue)
            {
                if (!aValue.isObject())
                    throw Error("etcd: not object value");
                key = Parser::Base64(aValue["key"].asString());
                value = Parser::Base64(aValue["value"].asString());
            }
        };
        using List = std::vector<Pair>;

    private:
        const Params m_Params;
        Curl::Client m_Client;

        Json::Value request(const std::string& aAPI, const std::string& aBody)
        {
            //BOOST_TEST_MESSAGE("etcd <- " << aAPI << ' ' << aBody);
            const Curl::Client::Request sRequest{
                .method  = Curl::Client::Method::POST,
                .url     = m_Params.url + "/v3/" + aAPI,
                .body    = aBody,
                .headers = {{"Accept", "application/json"}, {"Content-Type", "application/json"}}};
            auto sResult = m_Client(sRequest);
            //BOOST_TEST_MESSAGE("etcd -> " << sResult);
            return Protocol::parseResponse(sResult.status, sResult.body);
        }

    public:
        Client(const Params& aParams)
        : m_Params(aParams)
        {}

        std::string get(const std::string& aKey)
        {
            const auto sResult = request("kv/range", Protocol::get(m_Params.prefix + aKey));
            if (sResult.isObject() and sResult.isMember("kvs"))
                return Parser::Base64(sResult["kvs"][0]["value"].asString());
            return "";
        }

        void put(const std::string& aKey, const std::string& aValue, int64_t aLease = 0)
        {
            request("kv/put", Protocol::put(m_Params.prefix + aKey, aValue, aLease));
        }

        void remove(const std::string& aKey, bool aRange = false)
        {
            request("kv/deleterange", Protocol::remove(m_Params.prefix + aKey, aRange));
        }

        List list(const std::string& aKey, int64_t aLimit = 0)
        {
            const auto sResult = request("kv/range", Protocol::list(m_Params.prefix + aKey, aLimit));

            List sList;
            if (sResult.isObject()) {
                Parser::Json::from_value(sResult["kvs"], sList);
                for (auto& sItem : sList)
                    sItem.key.erase(0, m_Params.prefix.size());
                return sList;
            }
            return sList;
        }

        // lease api
        int64_t createLease(int32_t aTTL)
        {
            const auto sResult = request("lease/grant", Protocol::createLease(aTTL));
            return Parser::Atoi<int64_t>(sResult["ID"].asString());
        }

        void updateLease(int64_t aLease)
        {
            request("lease/keepalive", Protocol::leaseID(aLease));
        }

        void dropLease(int64_t aLease)
        {
            request("lease/revoke", Protocol::leaseID(aLease));
        }

        // txn
        void atomicPut(const std::string& aKey, const std::string& aValue)
        {
            const auto sResponse = request("kv/txn", Protocol::atomicPut(m_Params.prefix + aKey, aValue));
            Protocol::checkTxnResponse(sResponse);
        }

        void atomicUpdate(const std::string& aKey, const std::string& aOld, const std::string& aValue)
        {
            const auto sResponse = request("kv/txn", Protocol::atomicUpdate(m_Params.prefix + aKey, aOld, aValue));
            Protocol::checkTxnResponse(sResponse);
        }

        void atomicRemove(const std::string& aKey, const std::string& aOld)
        {
            const auto sResponse = request("kv/txn", Protocol::atomicRemove(m_Params.prefix + aKey, aOld));
            Protocol::checkTxnResponse(sResponse);
        }
    }; // namespace Etcd
} // namespace Etcd