#pragma once

#include <sstream>

#include <json/json.h>
#include <unsorted/Uuid.hpp>
#include <networking/Servername.hpp>
#include <sys/utsname.h>

#include "Backtrace.hpp"

// about protocol
// https://docs.sentry.io/development/sdk-dev/event-payloads/

namespace Sentry
{
    class Message
    {
        Json::Value m_Root;
    public:

        Message(const std::string& aLogger = "unspecified")
        {
            m_Root["event_id"] = Util::Uuid();
            m_Root["timestamp"] = Json::Value::Int64(time(nullptr));
            m_Root["logger"] = aLogger;
            m_Root["platform"] = "c";
            m_Root["server_name"] = Util::Servername();
        }

        // allowed: debug, info, warning, error, fatal
        Message& set_level(const std::string& v) { m_Root["level"] = v; return *this; }
        Message& set_environment(const std::string& v) { m_Root["environment"] = v; return *this; }
        Message& set_transaction(const std::string& v) { m_Root["transaction"] = v; return *this; }

        Message& set_message(const std::string& v)     { m_Root["message"]["message"] = v; return *this; }
        Message& set_module(const std::string& n, const std::string& v) { m_Root["modules"][n] = v; return *this; }
        Message& set_tag(const std::string& aName, const std::string& aValue) { m_Root["tags"][aName] = aValue; return *this; }
        Message& set_extra(const std::string& aName, const std::string& aValue) {m_Root["extra"][aName] = aValue; return *this;}

        Message& set_exception(const std::string& aType, const std::string& aValue, const std::string& aModule = "unknown") {
            auto& sX = m_Root["exception"];
            sX["type"] = aType;
            sX["value"] = aValue;
            sX["module"] = aModule;
            return *this;
        }

        Message& set_trace(const Trace& aTrace, const size_t aOffset = 2)
        {
            auto& sJson = m_Root["exception"]["stacktrace"]["frames"] = Json::arrayValue;
            ParseTrace(aTrace, [&sJson](auto aFrame){
                Json::Value sEntry;
                sEntry["function"] = aFrame.function;
                sEntry["filename"] = aFrame.filename;
                sEntry["lineno"] = Json::Value::Int64(aFrame.line);
                sEntry["instruction_addr"] = aFrame.addr;
                sJson.append(sEntry);
            }, aOffset);
            return *this;
        }

        struct Breadcrumb {
            time_t timestamp = 0;
            std::string level;     // fatal, error, warning, info, and debug
            std::string message;
            std::string category;
            std::map<std::string, std::string> aux;
        };

        // Breadcrumbs
        Message& log_work(const Breadcrumb& aWork)
        {
            auto& sJson = m_Root["breadcrumbs"]["values"];
            Json::Value sEntry;
            sEntry["timestamp"] = Json::Value::Int64(aWork.timestamp);
            sEntry["category"] = aWork.category;
            sEntry["message"] = aWork.message;
            sEntry["level"] = aWork.level;
            for (auto& [x,y] : aWork.aux)
                sEntry["data"][x] = y;
            sJson.append(sEntry);

            return *this;
        }

        Message& set_user(const std::string& aUser, const std::string& aAddress)
        {
            auto& sJson = m_Root["user"];
            sJson["id"] = aUser;
            sJson["ip_address"] = aAddress;
            return *this;
        }

        struct Request {
            std::string method = "GET";
            std::string url;
            std::map<std::string, std::string> aux;
        };

        Message& set_request(const Request& aRequest)
        {
            auto& sJson = m_Root["request"];
            sJson["method"] = aRequest.method;
            sJson["url"] = aRequest.url;
            for (auto& [x,y] : aRequest.aux)
                sJson["data"][x] = y;
            return *this;
        }

        Message& set_release(const std::string& v)     { m_Root["release"] = v; return *this; }
        Message& set_distrib(const std::string& v)     { m_Root["dist"] = v; return *this; }
        Message& set_version(const std::string& aName, const std::string& aVersion)
        {
            {
                auto& sJson = m_Root["contexts"]["app"];
                sJson["app_name"] = aName;
                sJson["app_version"] = aVersion;

                #ifdef __GNUC__
                const std::string sCompiler = "gcc";
                #else
                #ifdef __clang__
                const std::string sCompiler = "clang";
                #endif
                const std::string sCompiler = "unknown";
                #endif
                sJson["app_build"] = sCompiler + " " + __VERSION__ + " at " + __DATE__ + " " + __TIME__;
            }

            {
                struct utsname ver;
                uname(&ver);
                auto& sJson = m_Root["contexts"]["os"];
                sJson["name"]    = ver.sysname;
                sJson["version"] = ver.version;
                sJson["build"]   = ver.release;
            }

            return *this;
        }

        std::string to_string() const
        {
            Json::StreamWriterBuilder sBuilder;
            return Json::writeString(sBuilder, m_Root);
        }
    };
} // namespace Sentry