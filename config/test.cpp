#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Config.hpp"

#define FILE_NO_ARCHIVE
#include <file/File.hpp>

BOOST_AUTO_TEST_SUITE(Config)
BOOST_AUTO_TEST_CASE(parser)
{
    std::string sInput  = "foo.bar = a=b=c";
    auto        sResult = Config::PropertyFile::parse(sInput);
    BOOST_CHECK_EQUAL(sResult.first, "foo.bar");
    BOOST_CHECK_EQUAL(sResult.second, "a=b=c");
}
BOOST_AUTO_TEST_CASE(basic)
{
    Config::Manager sManager;
    BOOST_CHECK_EQUAL("mysql-master", sManager.get("mysql.host", "localhost"));
    BOOST_CHECK_EQUAL("foo", sManager.get("mysql.some.param", "foo"));
    BOOST_CHECK_THROW(sManager.get("mysql.some.param"), std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(ctx)
{
    {
        Config::Manager sManager;
        Config::Context sCtx1("main");
        {
            Config::Context sCtx1("master");
            BOOST_CHECK_EQUAL("mysql-master", sManager.get("mysql.host", "localhost"));
            BOOST_CHECK_EQUAL("foo", sManager.get("mysql.some.param", "foo"));
            BOOST_CHECK_THROW(sManager.get("mysql.some.param"), std::invalid_argument);

            // put param to manager
            sManager.properties().set("main:mysql.some.param", "from_main");
            BOOST_CHECK_EQUAL("from_main", sManager.get("mysql.some.param"));
            BOOST_CHECK_EQUAL(Config::Context::get(), "main.master");
        }
    }
    BOOST_CHECK_EQUAL(Config::Context::get(), "");
}
BOOST_AUTO_TEST_SUITE_END()