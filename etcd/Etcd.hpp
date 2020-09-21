#pragma once

#include <curl/Curl.hpp>
#include <exception/Error.hpp>
#include <file/Util.hpp>
#include <format/Hex.hpp>
#include <parser/Atoi.hpp>
#include <parser/Base64.hpp>

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

        Json::Value request(const std::string& aAPI, const std::string& aBody)
        {
            //BOOST_TEST_MESSAGE("etcd <- " << aAPI << ' ' << aBody);
            auto&& [sCode, sResult] = m_Client.POST(m_Params.url + "/v3/" + aAPI, aBody);
            //BOOST_TEST_MESSAGE("etcd -> " << sResult);
            return Protocol::parseResponse(sCode, sResult);
        }

    public:
        Client(const Params& aParams)
        : m_Params(aParams)
        , m_Client(m_CurlParams)
        {}

        std::string get(const std::string& aKey)
        {
            const auto sResult = request("kv/range", Protocol::get(m_Params.prefix + aKey));
            if (sResult.isObject())
                return decodePair(sResult["kvs"][0]).value;
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
            if (Protocol::isTransactionOk(sResponse))
                return;
            throw TxnError("transaction error for key: " + aKey);
        }

        void atomicUpdate(const std::string& aKey, const std::string& aOld, const std::string& aValue)
        {
            const auto sResponse = request("kv/txn", Protocol::atomicUpdate(m_Params.prefix + aKey, aOld, aValue));
            if (Protocol::isTransactionOk(sResponse))
                return;
            throw TxnError("transaction error for key: " + aKey);
        }

    }; // namespace Etcd
} // namespace Etcd