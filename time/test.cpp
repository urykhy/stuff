
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <Meter.hpp>
#include <Time.hpp>

BOOST_AUTO_TEST_SUITE(Time)
BOOST_AUTO_TEST_CASE(format_and_parse)
{
    time_t     sNow = ::time(nullptr); // utc time
    Time::Zone t(cctz::utc_time_zone());

    for (auto i = 1; i < Time::FORMAT_MAX; i++) {
        if (i == 1) {
            BOOST_CHECK_EQUAL(sNow, t.parse(t.format(sNow, Time::TIME)) + t.parse(t.format(sNow, Time::DATE)));
        } else {
            std::string sTmp = t.format(sNow, (Time::fmt)i);
            BOOST_CHECK_EQUAL(sNow, t.parse(sTmp));
        }
    }
}
BOOST_AUTO_TEST_CASE(simple)
{
    Time::Zone t(Time::load("Europe/Moscow"));
    BOOST_CHECK_EQUAL(t.parse("12:13:14"), 33194);
    BOOST_CHECK_EQUAL(t.parse("1970-01-01 01:02:03"), -7077);
    BOOST_CHECK_EQUAL(t.parse("1970-01-01 03:00:00"), 0);
    BOOST_CHECK_EQUAL(t.parse("2021-03-19T14:45:15.858Z"), 1616154315);
    BOOST_CHECK_EQUAL(t.to_time(-7077), cctz::civil_second(1970, 01, 01, 1, 2, 3));
    BOOST_CHECK_EQUAL(t.to_unix(cctz::civil_second(1970, 01, 01, 1, 2, 3)), -7077);
    BOOST_CHECK_EQUAL(t.to_date(-7077), cctz::civil_day(1970, 01, 01));
    BOOST_CHECK_EQUAL(t.format(-7077, Time::DATETIME), "1970-01-01 01:02:03");
    BOOST_CHECK_EQUAL(t.format(-7077, Time::ISO), "19700101010203");
    BOOST_CHECK_EQUAL(t.format(t.parse("1970-01-01 01:02:03"), Time::DATETIME), "1970-01-01 01:02:03");
    BOOST_CHECK_EQUAL(t.parse("Sun, 06 Nov 1994 08:49:37 GMT"), 784100977);
}
BOOST_AUTO_TEST_CASE(simple64)
{
    Time::Zone t(Time::load("Europe/Moscow"));

    const std::string sTime    = "2022-04-05T06:07:08.123456789Z";
    auto              sParsed  = t.parse64(sTime);
    auto [sSeconds, sUSeconds] = Time::Zone::split<std::chrono::seconds, std::chrono::microseconds>(sParsed);
    BOOST_CHECK_EQUAL(1649128028, sSeconds);
    BOOST_CHECK_EQUAL(123456, sUSeconds);
    BOOST_CHECK_EQUAL(sTime, t.format(sParsed, Time::ISO8601_F));
}
BOOST_AUTO_TEST_CASE(period)
{
    Time::Zone   t(cctz::utc_time_zone());
    Time::Period p(t, 600);

    BOOST_CHECK_EQUAL(p.round(t.to_time(7700)), t.to_time(7200));
    BOOST_CHECK_EQUAL(p.serial(t.to_time(7700)), 12);
}
BOOST_AUTO_TEST_CASE(meter)
{
    Time::Meter sMeter;
    sleep(1);
    BOOST_CHECK_CLOSE(sMeter.get().to_double(), 1, 0.1); // 0.1%
}
BOOST_AUTO_TEST_CASE(deadline)
{
    Time::Meter    sMeter;
    Time::Deadline sTimer(0.5);
    while (!sTimer.expired()) {
        const struct timespec sleep_time
        {
            0, 1 * 1000 * 1000
        };
        nanosleep(&sleep_time, nullptr);
    }
    BOOST_CHECK_CLOSE(sMeter.get().to_double(), 0.5, 1);
}
BOOST_AUTO_TEST_SUITE_END()
