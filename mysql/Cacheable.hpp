#pragma once
#include <fmt/core.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "Client.hpp"

#include <cache/Redis.hpp>
#include <format/List.hpp>
#include <time/Time.hpp>

namespace MySQL {

    struct CacheableQuery
    {
        std::string table;
        std::string from;
        std::string to;
        std::string where;
        std::string query;

        // pair: date, response row
        using Parse = std::function<std::pair<std::string, std::string>(const MySQL::Row& aRow)>;
        Parse parse;
    };

    class Cacheable
    {
        Time::Zone            m_Zone;
        Cache::Redis::Manager m_Redis;
        MySQL::Connection*    m_Connection;

    public:
        Cacheable(const Cache::Redis::Config aConfig, MySQL::Connection* aConnection)
        : m_Zone(cctz::utc_time_zone())
        , m_Redis(aConfig)
        , m_Connection(aConnection)
        {
        }

        using DateSet  = std::set<std::string>;              // dates to request from MySQL
        using Response = std::map<std::string, std::string>; // response: date -> row as string

        std::pair<DateSet, Response> prepare(const CacheableQuery& t)
        {
            const auto sFrom = m_Zone.to_date(m_Zone.parse(t.from));
            const auto sTo   = m_Zone.to_date(m_Zone.parse(t.to));

            DateSet  sMissing;
            Response sCached;

            // collect from cache
            std::vector<std::string> sDates;
            std::vector<std::string> sKeys;
            for (cctz::civil_day x = sFrom; x != sTo; x++) {
                const std::string sDay = m_Zone.format(x, Time::DATE);
                const std::string sKey = t.table + ":" + sDay + ":" + t.where;
                sKeys.push_back(sKey);
                sDates.push_back(sDay);
            }

            auto sTmp = m_Redis.mget(sKeys);
            if (sTmp.size() != sKeys.size())
                throw std::runtime_error("mysql/cacheable: inconsitent redis response");

            for (uint32_t i = 0; i < sTmp.size(); i++) {
                if (sTmp[i].has_value()) {
                    sCached[sDates[i]] = *sTmp[i];
                } else {
                    sMissing.insert(sDates[i]);
                }
            }

            return std::pair(sMissing, sCached);
        }

        Response operator()(const CacheableQuery& t)
        {
            auto [sMissing, sResponse] = prepare(t);

            // prepare query and run
            if (!sMissing.empty()) {
                std::stringstream sDates;
                Format::List(sDates, sMissing, [](const auto& x) { return "'" + x + "'"; });
                std::string sQuery = fmt::format(fmt::runtime(t.query), t.table, sDates.str(), t.where);
#ifdef BOOST_TEST_MESSAGE
                BOOST_TEST_MESSAGE("mysql cache query: " << sQuery);
#endif
                m_Connection->ensure();
                m_Connection->Query(sQuery);
                m_Connection->Use([&](const MySQL::Row& aRow) { sResponse.insert(t.parse(aRow)); });
            }

            // update cache
            for (auto& [sDate, sValue] : sResponse) {
                const std::string sKey = t.table + ":" + sDate + ":" + t.where;
                if (sMissing.contains(sDate)) {
                    m_Redis.set(sKey, sValue);
                } else {
                    m_Redis.expire(sKey);
                }
            }

            return sResponse;
        }
    };

} // namespace MySQL