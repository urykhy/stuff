#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "S3.hpp"

// minioc mb s3/test

BOOST_AUTO_TEST_SUITE(S3)
BOOST_AUTO_TEST_CASE(simple)
{
    S3::Params sParams;
    S3::API    sAPI(sParams);

    sAPI.PUT("some_file", "test data");
    auto sResult = sAPI.GET("some_file");

    bool sFound = false;
    auto sList = sAPI.LIST();
    for (auto& x : sList.keys)
    {
        BOOST_TEST_MESSAGE(fmt::format("{}\t{}\t{}", x.key, x.size, x.mtime));
        if (x.key == "some_file")
        {
            sFound = true;
            BOOST_CHECK_EQUAL(x.size, 9); // `test data`
        }
    }
    BOOST_CHECK_EQUAL(sList.truncated, false);
    BOOST_CHECK_EQUAL(sFound, true);

    try {
        auto sResult = sAPI.GET("some_nx_file");
    } catch (const S3::API::Error& e) {
        BOOST_TEST_MESSAGE(e.what());
    }

    BOOST_CHECK_EQUAL(sResult, "test data");
    sAPI.DELETE("some_file");
}
BOOST_AUTO_TEST_SUITE_END()
