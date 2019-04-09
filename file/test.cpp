/*
 *
 * g++ test.cpp -I. -I.. -lboost_unit_test_framework -lboost_system -lboost_filesystem
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <File.hpp>
#include <List.hpp>

BOOST_AUTO_TEST_SUITE(File)
BOOST_AUTO_TEST_CASE(read)
{
    BOOST_CHECK_EQUAL(File::to_string("__test_data1"), "123\nasd\n");

    File::by_string("__test_data1", [i = 0](const std::string& a) mutable {
        switch(i)
        {
        case 0: BOOST_CHECK_EQUAL(a, "123"); break;
        case 1: BOOST_CHECK_EQUAL(a, "asd"); break;
        default:
                BOOST_CHECK(false);
        }
        i++;
    });
}
BOOST_AUTO_TEST_CASE(list)
{
    auto sList = File::ReadDir(".", ".cpp");
    BOOST_CHECK(sList == File::FileList{"./test.cpp"});
}
BOOST_AUTO_TEST_CASE(name)
{
    const std::string sPath="/usr/bin/ls";
    BOOST_CHECK_EQUAL(File::get_filename(sPath), "ls");
    BOOST_CHECK_EQUAL(File::get_basename(sPath), "/usr/bin");
    BOOST_CHECK_EQUAL(File::get_extension("test.gif"), "gif");
}
BOOST_AUTO_TEST_CASE(glob)
{
    auto sList = File::Glob("./*.cpp");
    BOOST_CHECK(sList == File::FileList{"./test.cpp"});
}
BOOST_AUTO_TEST_SUITE_END()
