#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <HttpClient.hpp>

BOOST_AUTO_TEST_SUITE(Asio)
BOOST_AUTO_TEST_CASE(Beast)
{
    using namespace Asio::Http;

    net::io_context ioc;

    net::spawn(ioc, std::bind(
                              &HttpRequest,
                              "127.0.0.1",
                              "80",
                              "/",
                              10,
                              std::ref(ioc),
                              [](const Result& r){
                                  if (r.error)
                                      std::cerr << r.action << ": " << r.error.message() << std::endl;
                                  else
                                      std::cout << r.body;
                                  BOOST_CHECK_EQUAL((bool)r.error, false);
                                  BOOST_CHECK_EQUAL(r.body.result_int(), 200);
                              },
                              std::placeholders::_1));

    ioc.run();
}
BOOST_AUTO_TEST_SUITE_END()
