#pragma once

#include <json/json.h>

#include <exception/Error.hpp>
#include <format/Base64.hpp>
#include <format/Json.hpp>

namespace Etcd {
    struct EtcdTag;
    using Error = Exception::Error<EtcdTag>;

    struct TxnTag;
    using TxnError = Exception::Error<TxnTag, Error>;
} // namespace Etcd

namespace Etcd::Protocol {

    using namespace Format::Json;
    //inline const auto& to_string = Format::Json::to_string;

    inline std::string get(const std::string& aKey)
    {
        Json::Value sRoot;
        sRoot["key"] = Format::Base64(aKey);
        return to_string(sRoot);
    }

    inline std::string put(const std::string& aKey, const std::string& aValue, int64_t aLease = 0)
    {
        Json::Value sRoot;
        sRoot["key"]   = Format::Base64(aKey);
        sRoot["value"] = Format::Base64(aValue);
        if (aLease > 0)
            sRoot["lease"] = Json::Value::Int64(aLease);

        return to_string(sRoot);
    }

    inline std::string remove(const std::string& aKey, bool aRange)
    {
        Json::Value sRoot;
        sRoot["key"] = Format::Base64(aKey);
        if (aRange)
            sRoot["range_end"] = Format::Base64(aKey + "\xFF");

        return to_string(sRoot);
    }

    inline std::string list(const std::string& aKey, int64_t aLimit)
    {
        Json::Value sRoot;
        sRoot["key"]       = Format::Base64(aKey);
        sRoot["range_end"] = Format::Base64(aKey + "\xFF");
        if (aLimit > 0)
            sRoot["limit"] = Json::Value::Int64(aLimit);
        return to_string(sRoot);
    }

    inline std::string createLease(int32_t aTTL)
    {
        Json::Value sRoot;
        sRoot["TTL"] = Json::Value::Int64(aTTL);
        return to_string(sRoot);
    }

    inline std::string leaseID(int64_t aLease)
    {
        Json::Value sRoot;
        sRoot["ID"] = Json::Value::Int64(aLease);
        return to_string(sRoot);
    }

    inline std::string atomicPut(const std::string& aKey, const std::string& aValue)
    {
        Json::Value sRoot;

        auto& sCompare              = sRoot["compare"][0];
        sCompare["key"]             = Format::Base64(aKey);
        sCompare["result"]          = "EQUAL";
        sCompare["target"]          = "CREATE";
        sCompare["create_revision"] = 0;

        auto& sOp    = sRoot["success"][0]["request_put"];
        sOp["key"]   = Format::Base64(aKey);
        sOp["value"] = Format::Base64(aValue);

        return to_string(sRoot);
    }

    inline std::string atomicUpdate(const std::string& aKey, const std::string& aOld, const std::string& aValue)
    {
        Json::Value sRoot;

        auto& sCompare     = sRoot["compare"][0];
        sCompare["key"]    = Format::Base64(aKey);
        sCompare["result"] = "EQUAL";
        sCompare["target"] = "VALUE";
        sCompare["value"]  = Format::Base64(aOld);

        auto& sOp    = sRoot["success"][0]["request_put"];
        sOp["key"]   = Format::Base64(aKey);
        sOp["value"] = Format::Base64(aValue);

        return to_string(sRoot);
    }

    inline std::string atomicRemove(const std::string& aKey, const std::string& aOld)
    {
        Json::Value sRoot;

        auto& sCompare     = sRoot["compare"][0];
        sCompare["key"]    = Format::Base64(aKey);
        sCompare["result"] = "EQUAL";
        sCompare["target"] = "VALUE";
        sCompare["value"]  = Format::Base64(aOld);

        auto& sOp  = sRoot["success"][0]["request_delete_range"];
        sOp["key"] = Format::Base64(aKey);

        return to_string(sRoot);
    }

    inline Json::Value parseResponse(int aCode, const std::string& aBody)
    {
        if (aCode == 200) {
            Json::Value  sJson;
            Json::Reader sReader;
            if (!sReader.parse(aBody, sJson))
                throw Error("etcd: bad server response: " + sReader.getFormattedErrorMessages());
            return sJson;
        }
        throw Error("etcd: bad server response: http code: " + std::to_string(aCode) + ", message: " + aBody);
    }

    inline void checkTxnResponse(const Json::Value& aResponse)
    {
        if (aResponse.isObject() and aResponse["succeeded"].asBool())
            return;
        throw TxnError("etcd: transaction error");
    }

} // namespace Etcd::Protocol