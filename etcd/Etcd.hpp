#pragma once

#include <boost/noncopyable.hpp>

#include "Protocol.hpp"

#include <asio_http/Client.hpp>
#include <format/Hex.hpp>
#include <parser/Atoi.hpp>
#include <parser/Base64.hpp>
#include <parser/Json.hpp>
#include <unsorted/Env.hpp>

namespace Etcd {

    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;

    struct Client : boost::noncopyable
    {
        struct Params
        {
            std::string host    = Util::getEnv("ETCD_HOST");
            std::string prefix  = Util::getEnv("ETCD_PREFIX");
            time_t      timeout = 3; // seconds
        };

        struct Pair
        {
            std::string key;
            std::string value;

            void from_json(const Format::Json::Value& aValue)
            {
                if (!aValue.isObject())
                    throw Error("etcd: not object value");
                key   = Parser::Base64(aValue["key"].asString());
                value = Parser::Base64(aValue["value"].asString());
            }
        };
        using List = std::vector<Pair>;
        using Coro = std::optional<boost::asio::yield_context>;

    private:
        asio::io_service& m_Service;
        const Params      m_Params;
        Coro              m_Coro;

        Json::Value request(const std::string& aAPI, const std::string& aBody)
        {
            //BOOST_TEST_MESSAGE("etcd <- " << aAPI << ' ' << aBody);
            asio_http::ClientRequest sRequest{
                .method  = http::verb::post,
                .url     = m_Params.host + "/v3/" + aAPI,
                .body    = aBody,
                .headers = {{"Accept", "application/json"}, {"Content-Type", "application/json"}}};

            std::future<asio_http::Response> sFuture;
            if (m_Coro.has_value())
                sFuture = asio_http::async(m_Service, std::move(sRequest), *m_Coro);
            else
                sFuture = asio_http::async(m_Service, std::move(sRequest));

            if (sFuture.wait_for(std::chrono::seconds(m_Params.timeout)) != std::future_status::ready)
                throw std::runtime_error("etcd timeout");

            auto sResult = sFuture.get();
            //BOOST_TEST_MESSAGE("etcd -> " << sResult);
            return Protocol::parseResponse(sResult.result_int(), sResult.body());
        }

    public:
        Client(asio::io_service& aService, const Params& aParams, Coro aCoro = {})
        : m_Service(aService)
        , m_Params(aParams)
        , m_Coro(aCoro)
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