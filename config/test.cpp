#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Config.hpp"

#define FILE_NO_ARCHIVE
#include <file/File.hpp>
#include <file/Tmp.hpp>

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
BOOST_AUTO_TEST_CASE(wildcard)
{
    Config::Manager sManager;
    sManager.properties().set("*.timeout", "wild");
    {
        Config::Context sCtx1("main");
        {
            Config::Context sCtx1("master");
            BOOST_CHECK_EQUAL("wild", sManager.get("mysql.timeout"));
        }
    }
}
BOOST_AUTO_TEST_CASE(read_files)
{
    File::Tmp              sFile1("__a.prop");
    const std::string_view sData1 = "file.a = 123\nfoo.bar = ${MYSQL_HOST}";
    sFile1.write(sData1.data(), sData1.size());
    sFile1.flush();

    File::Tmp              sFile2("__b.prop");
    const std::string_view sData2 = "file.b = 456";
    sFile2.write(sData2.data(), sData2.size());
    sFile2.flush();

    setenv("PROPERTIES", (sFile1.name() + std::string(":") + sFile2.name()).c_str(), 1);
    Util::Raii      sCleanupProperties([]() { unsetenv("PROPERTIES"); });
    Config::Manager sManager;
    BOOST_CHECK_EQUAL("123", sManager.get("file.a"));
    BOOST_CHECK_EQUAL("456", sManager.get("file.b"));

    // check expand environment
    BOOST_CHECK_EQUAL("mysql-master", sManager.get("foo.bar"));
}
BOOST_AUTO_TEST_SUITE_END()