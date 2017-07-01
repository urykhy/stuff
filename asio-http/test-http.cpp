/*
 * _COPYRIGHT_
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <HttpClient.hpp>
#include <HttpClient2.hpp>
#include <thread>

using namespace Util;
using namespace HTTP;
log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("http"));

BOOST_AUTO_TEST_SUITE(Http)
BOOST_AUTO_TEST_CASE(simple)
{
    log4cxx::PropertyConfigurator::configure("logger.conf");

    ClientBase base;
    std::thread base_thread([base=&base](){base->run();});

    {
        Future<bool> wait;
        auto q = std::make_shared<HttpQuery>("GET / HTTP/1.0\r\n\r\n", [&wait](auto& res){
            TRACE("result callback called");
            try {
                auto r = res.get();

                TRACE("reply done: code:" << r.code);
                for (const auto& x : r.headers) {
                    TRACE(x.first << "=" << x.second);
                }
                TRACE("data: " << r.data);

            } catch (const std::exception& e) {
                ERROR("Exception: " << e.what());
            }
            wait.set_value(true);
        });

        auto http = std::make_shared<Query>(base.io_service, "127.0.0.1", 80, q);
        http->run();

        wait.get(std::chrono::seconds(1));
    }

    base.term();
    base_thread.join();
    //BOOST_CHECK(true);
}

auto make_query(const std::string url)
{
    return std::make_shared<HttpQuery>(url, [](auto& res){
        TRACE("result callback called");
        try {
            auto r = res.get();

            TRACE("reply done: code:" << r.code);
            for (const auto& x : r.headers) {
                TRACE(x.first << "=" << x.second);
            }
            TRACE("data: " << r.data);

        } catch (const std::exception& e) {
            ERROR("Exception: " << e.what());
        }
    });
}

BOOST_AUTO_TEST_CASE(proc)
{
    log4cxx::PropertyConfigurator::configure("logger.conf");

    ClientBase base;
    std::thread base_thread([base=&base](){base->run();});

    {
        AliveQuery::Queue queue(base.io_service);
        auto ru = std::make_shared<AliveQuery>(base.io_service, "127.0.0.1", 80, queue);
        ru->start();

        auto q1 = make_query("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
        auto q2 = make_query("GET /regex.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
        auto q3 = make_query("GET /index.html1 HTTP/1.1\r\nHost: localhost\r\n\r\n");

        queue.push(std::move(q1));
        queue.push(std::move(q2));

        sleep(6);
        //usleep(4500000);
        queue.push(std::move(q3));
        sleep(1);
    }

    base.term();
    base_thread.join();
}
BOOST_AUTO_TEST_SUITE_END()

