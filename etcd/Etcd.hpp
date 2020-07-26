#pragma once

#include <json/json.h>

#include <curl/Curl.hpp>
#include <file/Util.hpp>

namespace Etcd {
    struct Client
    {
        struct Params
        {
            std::string url    = "http://127.0.0.1:2379";
            std::string prefix = "test";
        };

        using Error = std::runtime_error;

    private:
        const Params         m_Params;
        Curl::Client::Params m_CurlParams;
        Curl::Client         m_Client;

        std::string url(const std::string& aKey, int aTTL = 0) const
        {
            return m_Params.url + "/v2/keys/" + m_Params.prefix + '/' + aKey + (aTTL > 0 ? "?ttl=" + std::to_string(aTTL) : "");
        }

        Json::Value parse(const std::string& aStr)
        {
            Json::Value  sRoot;
            Json::Reader sReader;
            if (!sReader.parse(aStr, sRoot))
                throw "fail to parse server response: " + sReader.getFormattedErrorMessages();
            return sRoot;
        }

        [[noreturn]] void report(const std::string& aAction, const std::string& aKey, int aCode, const std::string& aResult)
        {
            throw Error("etcd: fail to " + aAction + " " + aKey + ", http code: " + std::to_string(aCode) + ", message: " + aResult);
        }

    public:
        Client(const Params& aParams)
        : m_Params(aParams)
        , m_Client(m_CurlParams)
        {
            m_CurlParams.headers.push_back({"Content-Type", "application/x-www-form-urlencoded"});
        }

        struct Value
        {
            std::string value;
            uint64_t    modified = 0;
        };

        Value get(const std::string& aKey)
        {
            auto&& [sCode, sResult] = m_Client.GET(url(aKey));
            if (sCode == 200) {
                //BOOST_TEST_MESSAGE("etcd response: " << sResult);
                try {
                    const auto sRoot = parse(sResult);
                    return {sRoot["node"]["value"].asString(), sRoot["node"]["modifiedIndex"].asUInt64()};
                } catch (const std::exception& e) {
                    report("get", aKey, sCode, e.what());
                }
            }
            if (sCode == 404)
                return {};
            report("get", aKey, sCode, sResult);
        }

        void set(const std::string& aKey, const std::string& aValue, int aTTL = 0)
        {
            auto&& [sCode, sResult] = m_Client.PUT(url(aKey, aTTL), "value=" + aValue);
            if (sCode != 200 and sCode != 201)
                report("set", aKey, sCode, sResult);
        }

        void refresh(const std::string& aKey, int aTTL = 0)
        {
            std::string sData       = "refresh=true&prevExist=true";
            auto&& [sCode, sResult] = m_Client.PUT(url(aKey, aTTL), sData);
            if (sCode != 200)
                report("refresh", aKey, sCode, sResult);
        }

        void remove(const std::string& aKey)
        {
            auto&& [sCode, sResult] = m_Client.DELETE(url(aKey));
            if (sCode != 200)
                report("delete", aKey, sCode, sResult);
        }

        void cas(const std::string& aKey, const std::string& aOld, const std::string aNew)
        {
            std::string sParam;
            if (aOld.empty())
                sParam = "&prevExist=false";
            else
                sParam = "&prevValue=" + aOld;

            auto&& [sCode, sResult] = m_Client.PUT(url(aKey), "value=" + aNew + sParam);
            if (sCode != 200 and sCode != 201)
                report("cas", aKey, sCode, sResult);
        }

        void cad(const std::string& aKey, const std::string& aOld)
        {
            auto&& [sCode, sResult] = m_Client.DELETE(url(aKey) + "?prevValue=" + aOld);
            if (sCode != 200)
                report("cad", aKey, sCode, sResult);
        }

        struct Pair
        {
            std::string key;
            std::string value;
            uint64_t    modified = 0;
            bool        is_dir   = false;
            bool        operator<(const Pair& x) const { return key < x.key; }
        };

        using Set = std::vector<Pair>;

        Set list(const std::string& aKey)
        {
            Set sSet;

            auto&& [sCode, sResult] = m_Client.GET(url(aKey) + "?sorted=true");
            if (sCode != 200)
                report("list", aKey, sCode, sResult);

            try {
                const auto sRoot  = parse(sResult);
                auto&&     sNodes = sRoot["node"]["nodes"];
                for (Json::Value::ArrayIndex i = 0; i != sNodes.size(); i++) {
                    auto&& sNode = sNodes[i];
                    sSet.push_back({File::get_filename(sNode["key"].asString()),
                                    (sNode.isMember("value") ? sNode["value"].asString() : ""),
                                    sNode["modifiedIndex"].asUInt64(),
                                    sNode["dir"].asBool()});
                }
            } catch (const std::exception& e) {
                report("list", aKey, sCode, e.what());
            }

            return sSet;
        }

        void mkdir(const std::string& aKey)
        {
            auto&& [sCode, sResult] = m_Client.PUT(url(aKey), "dir=true");
            if (sCode != 200 and sCode != 201)
                report("create directory", aKey, sCode, sResult);
        }

        void enqueue(const std::string& aKey, const std::string& aValue)
        {
            auto&& [sCode, sResult] = m_Client.POST(url(aKey), "value=" + aValue);
            if (sCode != 201)
                report("enqueue", aKey, sCode, sResult);
        }

        void rmdir(const std::string& aKey)
        {
            auto&& [sCode, sResult] = m_Client.DELETE(url(aKey) + "?recursive=true");
            if (sCode != 200)
                report("delete directory", aKey, sCode, sResult);
        }
    };
} // namespace Etcd