#pragma once

#include <format/Json.hpp>
#include <parser/Json.hpp>

namespace SD {
    struct Entry
    {
        std::string key;
        double      latency     = 0;
        double      rps         = 0;
        uint32_t    threads     = 1;
        std::string location    = {};
        double      weight      = 0;
        double      utilization = 0;

        void from_json(const Format::Json::Value& aJson)
        {
            Parser::Json::from_object(aJson, "latency", latency);
            Parser::Json::from_object(aJson, "rps", rps);
            Parser::Json::from_object(aJson, "threads", threads);
            Parser::Json::from_object(aJson, "location", location);
            weight      = threads / latency;
            utilization = rps / weight;
        }
        Format::Json::Value to_json() const
        {
            Format::Json::Value sJson(::Json::objectValue);
            Format::Json::write(sJson, "latency", latency);
            Format::Json::write(sJson, "rps", rps);
            Format::Json::write(sJson, "threads", threads);
            Format::Json::write(sJson, "location", location);
            return sJson;
        }
    };
} // namespace SD