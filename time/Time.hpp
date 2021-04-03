#pragma once

#include <civil_time.h>
#include <time_zone.h>

#include <array>
#include <chrono>
#include <string>

namespace Time {
    using SC = std::chrono::system_clock;

    inline cctz::time_zone load(const std::string& aName)
    {
        cctz::time_zone sZone;
        if (!cctz::load_time_zone(aName, &sZone))
            throw std::runtime_error("fail to load zone");
        return sZone;
    }

    enum fmt
    {
        DATE,
        TIME,
        DATETIME,
        RFC1123,
        ISO,
        ISO8601_TZ,  // with T and Z
        ISO8601_LTZ, // with -T: and Z
        FORMAT_MAX
    };

    inline const std::string& formatString(fmt aFormat)
    {
        static const std::array<std::string, FORMAT_MAX> sDict{
            "%E4Y-%m-%d",                 // DATE
            "%H:%M:%S",                   // TIME
            "%E4Y-%m-%d %H:%M:%S",        // DATETIME
            "%a, %d %b %E4Y %H:%M:%S %Z", // RFC1123
            "%E4Y%m%d%H%M%S",             // ISO
            "%E4Y%m%dT%H%M%SZ",           // ISO8601_TZ
            "%E4Y-%m-%dT%H:%M:%SZ"};      // ISO8601_LTZ
        if (aFormat >= FORMAT_MAX)
            throw std::invalid_argument("formatString");
        return sDict[aFormat];
    }

    class Zone
    {
        const cctz::time_zone m_Zone;

    public:
        Zone(const cctz::time_zone aZone)
        : m_Zone(aZone)
        {}

        time_t parse(const std::string& aValue, fmt aFormat) const
        {
            SC::time_point sPoint;
            if (!cctz::parse(formatString(aFormat), aValue, m_Zone, &sPoint))
                return -1;
            return SC::to_time_t(sPoint);
        }

        time_t parse(const std::string& aValue) const
        {
            if (aValue.size() == 10 and aValue[4] == '-' and aValue[7] == '-')
                return parse(aValue, DATE);
            if (aValue.size() == 8 and aValue[2] == ':' and aValue[5] == ':')
                return parse(aValue, TIME);
            if (aValue.size() == 19 and aValue[4] == '-' and aValue[7] == '-' and aValue[10] == ' ' and aValue[13] == ':' and aValue[16] == ':')
                return parse(aValue, DATETIME);
            if (aValue.find(',') != std::string::npos)
                return parse(aValue, RFC1123);
            if (aValue.size() == 14 and std::all_of(aValue.begin(), aValue.end(), [](char a) { return std::isdigit(a); }))
                return parse(aValue, ISO);
            if (aValue.size() == 16 and aValue[8] == 'T' and aValue[15] == 'Z')
                return parse(aValue, ISO8601_TZ);
            if (aValue.size() == 20 and aValue[10] == 'T' and aValue[19] == 'Z')
                return parse(aValue, ISO8601_LTZ);
            if (aValue.size() > 20 and aValue[10] == 'T' and aValue[19] == '.' and aValue.back() == 'Z') {
                // 2021-03-19T14:45:15.858Z, discard fraction of seconds
                const std::string sTmp = aValue.substr(0, 19) + 'Z';
                return parse(sTmp, Time::ISO8601_LTZ);
            }
            return -1;
        }

        std::string format(time_t sPoint, fmt aFormat) const
        {
            return cctz::format(formatString(aFormat), SC::from_time_t(sPoint), m_Zone);
        }

        std::string format(const cctz::civil_second& aTime, fmt aFormat) const
        {
            return format(to_unix(aTime), aFormat);
        }

        cctz::civil_second to_time(time_t sPoint) const
        {
            return cctz::convert(SC::from_time_t(sPoint), m_Zone);
        }

        cctz::civil_day to_date(time_t sPoint) const
        {
            return cctz::civil_day{cctz::convert(SC::from_time_t(sPoint), m_Zone)};
        }

        time_t to_unix(const cctz::civil_second& aTime) const
        {
            return SC::to_time_t(cctz::convert(aTime, m_Zone));
        }
    };

    class Period
    {
        const Zone&    m_Zone;
        const unsigned m_Period;

    public:
        Period(const Zone& aZone, unsigned aPeriod)
        : m_Zone(aZone)
        , m_Period(aPeriod)
        {}

        cctz::civil_second round(const cctz::civil_second& aTime) const
        {
            auto sUnix = m_Zone.to_unix(aTime);
            sUnix -= sUnix % m_Period;
            return m_Zone.to_time(sUnix);
        }

        unsigned serial(const cctz::civil_second& aTime) const
        {
            auto sUnix = m_Zone.to_unix(aTime);
            return sUnix / m_Period;
        }
    };
} // namespace Time
