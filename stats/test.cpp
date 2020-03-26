#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Stat.hpp"
#include "Common.hpp"
#include "API.hpp"
#include <networking/Servername.hpp>
#include <asio_http/Server.hpp>

BOOST_AUTO_TEST_SUITE(Stat)
BOOST_AUTO_TEST_CASE(counter)
{
    Stat::Counter sCounter("test.counter","test{some=\"value\"}");
    sCounter.set(10);

    const auto sActual = Stat::Manager::instance().toPrometheus();
    const Stat::Manager::Set sExpected={{"test{some=\"value\"} 10"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sActual.begin(), sActual.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(ago)
{
    time_t sNow = ::time(nullptr);
    Stat::Age sAge("test.counter","test{some=\"value\"}");
    sAge.set(sNow);
    BOOST_CHECK_EQUAL(sAge.format(), std::to_string(0));
}
BOOST_AUTO_TEST_CASE(time)
{
    Stat::Time sTime("test.time", "time");
    for (float i = 0; i <= 10; i+=0.001)
        sTime.account(i);
    sTime.update();

    const auto sActual = Stat::Manager::instance().toPrometheus();
    const Stat::Manager::Set sExpected={
        {"time{quantile=\"0.5\"} 5.000000"}
      , {"time{quantile=\"0.9\"} 9.000000"}
      , {"time{quantile=\"0.99\"} 9.900000"}
      , {"time{quantile=\"1.0\"} 9.999000"}
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(sActual.begin(), sActual.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(common)
{
    Stat::Common sCommon;
    sCommon.update();
    BOOST_TEST_MESSAGE("   rss: " << sCommon.m_RSS.format());
    BOOST_TEST_MESSAGE("  user: " << sCommon.m_User.format());
    BOOST_TEST_MESSAGE("system: " << sCommon.m_System.format());
    BOOST_TEST_MESSAGE("    fd: " << sCommon.m_FDS.format());
}
#if 0
BOOST_AUTO_TEST_CASE(real)
{
    Stat::Config sConfig;
    sConfig.host="graphite.grafana.docker";
    sConfig.period = 5;
    sConfig.prefix="test." + Util::Servername();

    auto sRouter = std::make_shared<asio_http::Router>();
    Stat::prometheusRouter(sRouter);

    Threads::Asio sAsio;
    asio_http::startServer(sAsio, 2081, sRouter);

    Threads::Group sGroup;
    Stat::start(sGroup, sConfig);
    sAsio.start(1, sGroup);

    sleep(600);
}
#endif
BOOST_AUTO_TEST_SUITE_END()
