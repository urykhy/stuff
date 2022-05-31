#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#define ASIO_HTTP_LIBRARY_HEADER
#include "Alive.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "v2/Client.hpp"
#include "v2/Server.hpp"

BOOST_AUTO_TEST_SUITE(asio_http)
BOOST_AUTO_TEST_CASE(basic)
{
    Threads::Asio sAsio;

    auto a = asio_http::makeClient(sAsio.service());
    (void)a;
    a = asio_http::v1::makeClient(sAsio.service());
    (void)a;
    a = asio_http::v2::makeClient(sAsio.service());
    (void)a;

    auto sRouter = std::make_shared<asio_http::Router>();
    asio_http::startServer(sAsio.service(), 2081, sRouter);
    asio_http::v1::startServer(sAsio.service(), 2082, sRouter);
    asio_http::v2::startServer(sAsio.service(), 2083, sRouter);
}
BOOST_AUTO_TEST_SUITE_END()