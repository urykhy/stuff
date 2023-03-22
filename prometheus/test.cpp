#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "API.hpp"
#include "Common.hpp"
#include "GetOrCreate.hpp"
#include "Metrics.hpp"
#include "Notice.hpp"

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
BOOST_AUTO_TEST_CASE(histogramm)
{
    Prometheus::Histogramm sH;
    {
        sH.tick(1);
        sH.tick(2);
        sH.tick(3);
        BOOST_CHECK_CLOSE(sH.quantile(std::array<double, 1>{0.5})[0], 2, 0.01);
        sH.clear();
    }
    {
        sH.tick(1);
        sH.tick(1);
        sH.tick(3);
        sH.tick(3);
        BOOST_CHECK_CLOSE(sH.quantile(std::array<double, 1>{0.5})[0], 2, 0.01);
        sH.clear();
    }
}
BOOST_AUTO_TEST_CASE(time)
{
    Prometheus::Time sTime("time");
    for (unsigned i = 1; i <= 100; i++)
        sTime.account(i * 0.1);
    sTime.update();

    const auto                     sActual   = Prometheus::Manager::instance().toPrometheus();
    const Prometheus::Manager::Set sExpected = {
        {"time{quantile=\"0.5\"} 5.050000"}, {"time{quantile=\"0.9\"} 8.950000"}, {"time{quantile=\"0.99\"} 9.850000"}, {"time{quantile=\"1.0\"} 10.000000"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sActual.begin(), sActual.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(router)
{
    auto sRouter = std::make_shared<asio_http::Router>();
    Prometheus::configure(sRouter);
    Threads::Asio sAsio;
    asio_http::startServer(sAsio.service(), 2081, sRouter);

    Threads::Group sGroup;
    Prometheus::start(sGroup);
    sAsio.start(sGroup);

    asio_http::ClientRequest sRequest{.method = asio_http::http::verb::get, .url = "http://127.0.0.1:2081/metrics"};

    auto sResponse = asio_http::async(sAsio.service(), std::move(sRequest)).get();
    BOOST_CHECK_EQUAL(sResponse.result(), asio_http::http::status::ok);
    BOOST_TEST_MESSAGE("metrics: \n"
                       << sResponse.body());
}
BOOST_AUTO_TEST_CASE(get_or_create)
{
    using T = Prometheus::Counter<>;
    Prometheus::GetOrCreate sStore;
    sStore.get<T>("rps")->set(10);
    sStore.get<T>("rps")->tick();
    BOOST_CHECK_EQUAL(sStore.get<T>("rps")->format(), "11");
}
BOOST_AUTO_TEST_CASE(notice)
{
    Prometheus::Notice::Key sKey{.message = "connection error"};

    auto sTest = [](std::string_view aVal, unsigned aExpected = 1) {
        unsigned   sCount  = 0;
        const auto sActual = Prometheus::Manager::instance().toPrometheus();
        for (auto x : sActual) {
            BOOST_TEST_MESSAGE(x);
            if (x == aVal)
                sCount++;
        }
        BOOST_CHECK_EQUAL(sCount, aExpected);
    };

    Prometheus::Notice::set(sKey);
    sTest(R"(status{priority="notice",message="connection error"} 1)");
    Prometheus::Notice::reset(sKey);
    sTest(R"(status{priority="notice",message="connection error"} 0)", 0);

    Prometheus::Notice::flag("metrics_up");
    sTest(R"(metrics_up 1)");
}
BOOST_AUTO_TEST_SUITE_END()
