#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <boost/test/data/test_case.hpp>

#include "Block.hpp"
#include "Dir.hpp"
#include "File.hpp"
#include "Tmp.hpp"

const std::vector<std::string> sExt{"txt", "gz", "bz2", "xz", "lz4", "zst"};

BOOST_AUTO_TEST_SUITE(FileSuite)
BOOST_DATA_TEST_CASE(read_write, sExt)
{
    const std::string sData = "123\n123\n321\n\n";
    std::string       sReaded;
    const std::string sName = "__test." + sample;

    File::write(sName, [&sData](File::IWriter* aWriter) {
        aWriter->write(sData.data(), sData.size());
    });

    File::read(sName, [&sReaded](File::IReader* aReader) {
        sReaded.resize(128);
        size_t sActual = aReader->read(sReaded.data(), sReaded.size());
        sReaded.resize(sActual);
        BOOST_CHECK(aReader->eof());
    });

    BOOST_CHECK_EQUAL(sReaded, sData);

    sReaded = File::to_string(sName);
    BOOST_CHECK_EQUAL(sReaded, sData);

    unsigned sSerial = 0;
    File::by_string(sName, [&sSerial](const std::string_view aStr) mutable {
        switch (sSerial) {
        case 0: BOOST_CHECK_EQUAL(aStr, "123"); break;
        case 1: BOOST_CHECK_EQUAL(aStr, "123"); break;
        case 2: BOOST_CHECK_EQUAL(aStr, "321"); break;
        case 3: BOOST_CHECK_EQUAL(aStr, ""); break;
        }
        sSerial++;
    });
    BOOST_CHECK_EQUAL(sSerial, 4);
}

BOOST_AUTO_TEST_CASE(block)
{
    const std::string sName = "__block.bin";
    File::Block::write(sName, [](auto aApi) {
        aApi->write(0, "hello");
        aApi->write(1, "world");
    });

    unsigned sSerial = 0;
    File::Block::read(sName, [&sSerial](uint8_t aType, const std::string& aData) mutable {
        switch (sSerial) {
        case 0:
            BOOST_CHECK_EQUAL(aType, 0);
            BOOST_CHECK_EQUAL(aData, "hello");
            break;
        case 1:
            BOOST_CHECK_EQUAL(aType, 1);
            BOOST_CHECK_EQUAL(aData, "world");
            break;
        }
        sSerial++;
    });
    BOOST_CHECK_EQUAL(sSerial, 2);
}
BOOST_AUTO_TEST_CASE(tmp)
{
    std::string sTmpName;
    {
        File::Tmp sTmp("some_test_file.txt");
        sTmp.write("some data", 10);
        sTmp.flush();
        sTmpName = sTmp.name();
        BOOST_TEST_MESSAGE("tmp filename is " << sTmpName);
        BOOST_CHECK_EQUAL(File::getExtension(sTmpName), "txt");
        BOOST_CHECK_EQUAL(sTmp.size(), 10);
    }
    BOOST_CHECK_EQUAL(std::filesystem::exists(sTmpName), false);
}

BOOST_AUTO_TEST_SUITE(Dir)
BOOST_AUTO_TEST_CASE(list)
{
    auto sList = File::listFiles("..", ".cpp");
    BOOST_CHECK(sList == File::FileList{"../test.cpp"});
}
BOOST_AUTO_TEST_CASE(util)
{
    const std::string sPath = "/usr/bin/ls";
    BOOST_CHECK_EQUAL(File::getFilename(sPath), "ls");
    BOOST_CHECK_EQUAL(File::getBasename(sPath), "/usr/bin");
    BOOST_CHECK_EQUAL(File::getBasename("ls"), "");
    BOOST_CHECK_EQUAL(File::getExtension("test.gif"), "gif");
    BOOST_CHECK_EQUAL(File::getExtension("test.gif.tmp-123456"), "gif");
    BOOST_CHECK_EQUAL(File::getExtension(".test.gif.tmp"), "gif");
    BOOST_CHECK_EQUAL(File::getExtension("gif.tmp-123456"), "");
    BOOST_CHECK_EQUAL(File::getExtension("test.gif.tmp-123456", false), "tmp-123456");
    BOOST_CHECK_EQUAL(File::tmpName("/foo/bar/name"), "/foo/bar/.name.tmp");
}
BOOST_AUTO_TEST_CASE(glob)
{
    auto sList = File::glob("../*.cpp");
    BOOST_CHECK(sList == File::FileList{"../test.cpp"});
}
BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
