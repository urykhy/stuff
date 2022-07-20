#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include "Server.hpp"

#include <mysql/Client.hpp>

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

            if (aQuery == "select @@version_comment limit 1") {
                ResultSet sResult;
                sResult.add_column("@@version_comment");
                sResult.add_row().append("proof-of-concept");
                return sResult.write(aStream, yield);
            }
            if (aQuery == "select USER()") {
                ResultSet sResult;
                sResult.add_column("USER()");
                sResult.add_row().append("manager");
                return sResult.write(aStream, yield);
            }
            if (aQuery == "select * from test") {
                ResultSet sResult;
                sResult.add_column("id").column_type = MYSQL_TYPE_LONG;
                sResult.add_column("label");
                sResult.add_row().append("1").append("one");
                sResult.add_row().append("2").append("many");
                sResult.add_row().append("3").append();
                return sResult.write(aStream, yield);
            }
            OkResponse             sOk;
            asio_mysql::omemstream sStream;
            sOk.serialize(sStream);
            return write(aStream, yield, sStream.str());
        }
    };
    auto sImpl = std::make_shared<Impl>();

    Threads::Asio  sAsio;
    Threads::Group sGroup;
    asio_mysql::startServer(sAsio, 3306, sImpl);
    sAsio.start(sGroup);

    std::this_thread::sleep_for(50ms);

    MySQL::Connection sClient({.host = "127.0.0.1", .password = ""});
    sClient.Query("select @@version_comment limit 1");
    sClient.Use([](const MySQL::Row& aRow) {
        BOOST_CHECK_EQUAL("proof-of-concept", aRow[0].as_string());
    });
    sClient.Query("select USER()");
    sClient.Use([](const MySQL::Row& aRow) {
        BOOST_CHECK_EQUAL("manager", aRow[0].as_string());
    });
    sClient.Query("select * from test");
    sClient.Use([sCount = 0](const MySQL::Row& aRow) mutable {
        sCount++;
        switch (sCount) {
        case 1:
            BOOST_CHECK_EQUAL(1, aRow[0].as_int64());
            BOOST_CHECK_EQUAL("one", aRow[1].as_string());
            break;
        case 3:
            BOOST_CHECK_EQUAL(3, aRow[0].as_int64());
            BOOST_CHECK_EQUAL(true, aRow[1].is_null());
        };
    });
}
BOOST_AUTO_TEST_SUITE_END()
