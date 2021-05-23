#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "API.hpp"
#include "Common.hpp"
#include "Metrics.hpp"

#include <asio_http/Client.hpp>
#include <asio_http/Server.hpp>
#include <networking/Servername.hpp>

BOOST_AUTO_TEST_SUITE(Prometheus)
BOOST_AUTO_TEST_CASE(counter)
{
    Prometheus::Counter sCounter("test{some=\"value\"}");
    sCounter.set(10);

    const auto                     sActual   = Prometheus::Manager::instance().toPrometheus();
    const Prometheus::Manager::Set sExpected = {{"test{some=\"value\"} 10"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sActual.begin(), sActual.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(ago)
{
    time_t          sNow = ::time(nullptr);
    Prometheus::Age sAge("test{some=\"value\"}");
    sAge.set(sNow);
    BOOST_CHECK_EQUAL(sAge.format(), std::to_string(0));
}
BOOST_AUTO_TEST_CASE(time)
{
    Prometheus::Time sTime("time");
    for (unsigned i = 1; i <= 100; i++)
        sTime.account(i * 0.1);
    sTime.update();

    const auto                     sActual   = Prometheus::Manager::instance().toPrometheus();
    const Prometheus::Manager::Set sExpected = {
        {"time{quantile=\"0.5\"} 5.100000"}, {"time{quantile=\"0.9\"} 9.000000"}, {"time{quantile=\"0.99\"} 9.900000"}, {"time{quantile=\"1.0\"} 10.000000"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sActual.begin(), sActual.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(router)
{
    auto sRouter = std::make_shared<asio_http::Router>();
    Prometheus::configure(sRouter);
    Threads::Asio sAsio;
    asio_http::startServer(sAsio, 2081, sRouter);

    Threads::Group sGroup;
    Prometheus::start(sGroup);
    sAsio.start(sGroup);

    asio_http::ClientRequest sRequest{.method = asio_http::http::verb::get, .url = "http://127.0.0.1:2081/metrics"};

    auto sResponse = asio_http::async(sAsio, std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_TEST_MESSAGE("metrics: \n"
                       << sResponse.body());
}
BOOST_AUTO_TEST_SUITE_END()
