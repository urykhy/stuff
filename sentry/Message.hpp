#pragma once

#include <sstream>

#include <json/json.h>
#include <unsorted/Backtrace.hpp>
#include <unsorted/Uuid.hpp>
#include <networking/Servername.hpp>

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

        Message& set_transaction(const std::string& v) { m_Root["transaction"] = v; return *this; }
        Message& set_release(const std::string& v)     { m_Root["release"] = v; return *this; }
        Message& set_distrib(const std::string& v)     { m_Root["dist"] = v; return *this; }
        Message& set_environment(const std::string& v) { m_Root["environment"] = v; return *this; }
        Message& set_message(const std::string& v)     { m_Root["message"]["message"] = v; return *this; }
        Message& set_tag(const std::string& aName, const std::string& aValue) { m_Root["tags"][aName] = aValue; return *this; }

        Message& set_exception(const std::string& aType, const std::string& aValue, const std::string& aModule = "unknown") {
            auto& sX = m_Root["exception"];
            sX["type"] = aType;
            sX["value"] = aValue;
            sX["module"] = aModule;
            return *this;
        }

        Message& set_trace(const Util::Stacktrace& aTrace)
        {
            auto& sJson = m_Root["exception"]["stacktrace"]["frames"] = Json::arrayValue;
            Util::ParseStacktrace(aTrace, [&sJson](auto aFrame){
                Json::Value sEntry;
                sEntry["function"] = aFrame.function;
                sEntry["filename"] = aFrame.filename;
                sEntry["lineno"] = Json::Value::Int64(aFrame.line);
                sEntry["instruction_addr"] = aFrame.addr;
                sJson.append(sEntry);
            });
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
                sEntry["data"][x]=y;
            sJson.append(sEntry);

            return *this;
        }

        std::string to_string() const
        {
            Json::StreamWriterBuilder sBuilder;
            return Json::writeString(sBuilder, m_Root);
        }
    };
} // namespace Sentry