#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include "Server.hpp"

BOOST_AUTO_TEST_SUITE(asio_mysql)
BOOST_AUTO_TEST_CASE(simple)
{
    struct Impl : public asio_mysql::ImplFace
    {
        beast::error_code process(beast::tcp_stream& aStream,
                                  net::yield_context yield,
                                  const std::string& aQuery) override
        {
            BOOST_TEST_MESSAGE("got query: " << aQuery);
            beast::error_code ec;
            Container::binary sBuffer;

            if (aQuery == "select @@version_comment limit 1")
            {
                ResultSet sResult;
                sResult.add_column("@@version_comment");
                sResult.add_row().append("proof-of-concept");
                return sResult.write(aStream, yield);
            }
            if (aQuery == "select USER()")
            {
                ResultSet sResult;
                sResult.add_column("USER()");
                sResult.add_row().append("manager");
                return sResult.write(aStream, yield);
            }
            if (aQuery == "select * from test")
            {
                ResultSet sResult;
                sResult.add_column("id").column_type = MYSQL_TYPE_LONG;
                sResult.add_column("label");
                sResult.add_row().append("1").append("one");
                sResult.add_row().append("2").append("many");
                sResult.add_row().append("3").append();
                return sResult.write(aStream, yield);
            }
            OkResponse sOk;
            sOk.serialize(sBuffer);
            return write(aStream, yield, sBuffer);
        }
        ~Impl() override {}
    };
    auto sImpl = std::make_shared<Impl>();

    Threads::Asio sAsio;
    Threads::Group sGroup;
    asio_mysql::startServer(sAsio, 3306, sImpl);
    sAsio.start(sGroup);

    std::this_thread::sleep_for(600s);
}
BOOST_AUTO_TEST_SUITE_END()
