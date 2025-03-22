#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "S3.hpp"

// minioc mb s3/test

BOOST_AUTO_TEST_SUITE(S3)
BOOST_AUTO_TEST_CASE(simple)
{
    time_t     sNow = time(nullptr);
    S3::Params sParams;
    S3::API    sAPI(sParams);

    std::string_view  sContent = "test data";
    const std::string sHash    = SSLxx::DigestStr(EVP_sha256(), sContent);
    sAPI.PUT("some_file", sContent, sHash);
    auto sResult = sAPI.GET("some_file");

    bool sFound = false;
    auto sList  = sAPI.LIST();
    for (auto& x : sList.keys) {
        BOOST_TEST_MESSAGE(fmt::format("{}\t{}\t{}", x.key, x.size, x.mtime));
        if (x.key == "some_file") {
            sFound = true;
            BOOST_CHECK_EQUAL(x.size, sContent.size());
            break;
        }
    }
    //BOOST_CHECK_EQUAL(sList.truncated, false);
    BOOST_CHECK_EQUAL(sFound, true);

    auto sHead = sAPI.HEAD("some_file");
    BOOST_CHECK_EQUAL(sHead.status, 200);
    BOOST_CHECK_EQUAL(sHead.size, sContent.size());
    BOOST_CHECK_GE(sHead.mtime, sNow);
    BOOST_CHECK_EQUAL(sHead.sha256, sHash);

    try {
        auto sResult = sAPI.GET("some_nx_file");
    } catch (const S3::API::Error& e) {
        BOOST_TEST_MESSAGE(e.what());
    }

    BOOST_CHECK_EQUAL(sResult, sContent);
    sAPI.DELETE("some_file");
}
BOOST_AUTO_TEST_CASE(multipart)
{
    S3::Params sParams;
    S3::API    sAPI(sParams);

    // Each part must be at least 5 MB in size
    auto sGenerator = [serial = 0]() mutable -> std::string {
        serial++;
        switch (serial) {
        case 1: return std::string(5 * 1024 * 1024, 'a');
        case 2: return std::string(5 * 1024 * 1024, 'b');
        case 3: return std::string(5 * 1024 * 1024, 'c');
        case 4: return std::string(1 * 1024 * 1024, 'd');
        default: return "";
        };
    };

    // Cals hash...
    const auto sHash = [sGenerator]() mutable {
        std::string sBuf;
        while (true) {
            auto sTmp = sGenerator();
            if (sTmp.empty())
                break;
            sBuf += sTmp;
        }
        return SSLxx::DigestStr(EVP_sha256(), sBuf);
    }();

    sAPI.multipartPUT("some_multipart", sGenerator, sHash);

    auto sData = sAPI.GET("some_multipart");
    BOOST_CHECK_EQUAL(sData.size(), (5 * 3 + 1) * 1024 * 1024);

    auto sHead = sAPI.HEAD("some_multipart");
    BOOST_CHECK_EQUAL(sHead.size, (5 * 3 + 1) * 1024 * 1024);
    BOOST_CHECK_EQUAL(sHead.sha256, sHash);
    BOOST_CHECK_EQUAL(sHead.parts, 4);

    // HEAD by part-number
    sHead = sAPI.HEAD("some_multipart", 4);
    BOOST_CHECK_EQUAL(sHead.size, 1 * 1024 * 1024);

    // GET by part-number
    auto sPartData = sAPI.GET("some_multipart", 4);
    BOOST_CHECK_EQUAL(sPartData, std::string(1 * 1024 * 1024, 'd'));

    // cleanup
    sAPI.DELETE("some_multipart");
}
BOOST_AUTO_TEST_CASE(csv)
{
    S3::Params sParams;
    S3::API    sAPI(sParams);

    // put XML file
    const std::string sContent = R"("name","message"
c1,foo
c2,bar
c3,boo
    )";
    sAPI.PUT("test.csv", sContent);

    // make SELECT
    std::string sResult = sAPI.SELECT("test.csv", "SELECT s.message FROM S3Object s WHERE s.name='c2'");
    BOOST_CHECK_EQUAL("bar\n", sResult);

    // bad query
    BOOST_CHECK_THROW(sAPI.SELECT("test.csv", "SELECT s.nx_column FROM S3Object s WHERE s.name='c2'"), std::invalid_argument);

    // cleanup
    sAPI.DELETE("test.csv");
}
BOOST_AUTO_TEST_SUITE_END()
