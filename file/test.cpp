#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include "File.hpp"
#include "List.hpp"
#include "Tmp.hpp"
#include "Block.hpp"

BOOST_AUTO_TEST_SUITE(File)
BOOST_AUTO_TEST_CASE(read)
{
    BOOST_CHECK_EQUAL(File::to_string("../__test.lz4"), "123\n123\n321\n\n");

    File::by_string("../__test.lz4", [i = 0](const std::string& a) mutable {
        switch(i)
        {
        case 0: BOOST_CHECK_EQUAL(a, "123"); break;
        case 1: BOOST_CHECK_EQUAL(a, "123"); break;
        case 2: BOOST_CHECK_EQUAL(a, "321"); break;
        case 3: BOOST_CHECK_EQUAL(a, ""); break;
        default:
                BOOST_CHECK(false);
        }
        i++;
    });
}
BOOST_AUTO_TEST_CASE(list)
{
    auto sList = File::ReadDir("..", ".cpp");
    BOOST_CHECK(sList == File::FileList{"../test.cpp"});
}
BOOST_AUTO_TEST_CASE(util)
{
    const std::string sPath="/usr/bin/ls";
    BOOST_CHECK_EQUAL(File::get_filename(sPath), "ls");
    BOOST_CHECK_EQUAL(File::get_basename(sPath), "/usr/bin");
    BOOST_CHECK_EQUAL(File::get_basename("ls"), "");
    BOOST_CHECK_EQUAL(File::get_extension("test.gif"), "gif");
}
BOOST_AUTO_TEST_CASE(glob)
{
    auto sList = File::Glob("../*.cpp");
    BOOST_CHECK(sList == File::FileList{"../test.cpp"});
}
BOOST_AUTO_TEST_CASE(tmp)
{
    std::string sTmpName;
    {
        File::Tmp sTmp("some_test_file");
        sTmp.write("some data", 10);
        sTmpName = sTmp.filename();
        BOOST_TEST_MESSAGE("tmp filename is " << sTmpName);
        BOOST_CHECK_EQUAL(sTmp.size(), 10);
    }
    BOOST_CHECK_EQUAL(std::filesystem::exists(sTmpName), false);
}
BOOST_AUTO_TEST_CASE(block)
{
    const std::vector<std::string> sData = {{"Lorem Ipsum is simply dummy text of the printing and typesetting industry."},
                                            {"Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, "},
                                            {"when an unknown printer took a galley of type and scrambled it to make a type specimen book. "},
                                            {"It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged."},
                                            {" It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, "},
                                            {"and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum."}
                                           };
    std::string sExpected;
    const std::string sName = "__test_data.zst";
    File::Block::Writer sWriter(sName);
    for (auto& x : sData)
    {
        sExpected += x;
        sWriter(0, x);
    }
    sWriter.close();
    BOOST_CHECK(std::filesystem::exists(sName));

    std::string sActual;
    File::Block::Reader(sName, [&sActual](uint8_t aType, std::string_view aData){
        BOOST_CHECK_EQUAL(aType, 0);
        sActual += aData;
    });
    BOOST_CHECK_EQUAL(sExpected, sActual);

    std::filesystem::remove(sName);
}
BOOST_AUTO_TEST_SUITE_END()
