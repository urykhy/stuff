#include <civil_time.h>
#include <time_zone.h>

#include <array>
#include <chrono>
#include <string>

namespace Time
{
    using SC = std::chrono::system_clock;

    inline cctz::time_zone load(const std::string& aName)
    {
        cctz::time_zone sZone;
        if (!cctz::load_time_zone(aName, &sZone))
            throw "fail to load zone";
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
        ISO8601_LTZ, // with -,:,T and Z
        FORMAT_MAX
    };

    inline const std::string& formatString(fmt aFormat)
    {
        static std::array<std::string, FORMAT_MAX> sDict {
            "%E4Y-%m-%d",
            "%H:%M:%S",
            "%E4Y-%m-%d %H:%M:%S",
            "%a, %d %b %E4Y %H:%M:%S %Z",
            "%E4Y%m%d%H%M%S",
            "%E4Y%m%dT%H%M%SZ",
            "%E4Y-%m-%dT%H:%M:%SZ"
        };
        if (aFormat >= FORMAT_MAX)
            throw "formatString";
        return sDict[aFormat];
    }

    class Zone
    {
        const cctz::time_zone m_Zone;
    public:
        Zone(const cctz::time_zone aZone) : m_Zone(aZone) { }

        time_t parse(const std::string& aValue, fmt aFormat) const
        {
            SC::time_point sPoint;
            if (!cctz::parse(formatString(aFormat), aValue, m_Zone, &sPoint))
                return -1;
            return SC::to_time_t(sPoint);
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
        const Zone& m_Zone;
        const unsigned m_Period;
    public:
        Period (const Zone& aZone, unsigned aPeriod) : m_Zone(aZone), m_Period(aPeriod) {}

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
}
