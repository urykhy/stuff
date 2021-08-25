#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

#include "Get.hpp"
#include "Tar.hpp"

DECLARE_RESOURCE(sample_data, "../sample_data.data")
DECLARE_RESOURCE(sample_tar, "../sample_tar.tar")

BOOST_AUTO_TEST_SUITE(resource)
BOOST_AUTO_TEST_CASE(simple)
{
    const auto sTmp = resource::sample_data();
    BOOST_TEST_MESSAGE(sTmp);
    BOOST_CHECK_EQUAL(sTmp, "some data\nto inject\ninto elf\n");
}
BOOST_AUTO_TEST_CASE(tar)
{
    resource::Tar sTar(resource::sample_tar());
    BOOST_CHECK_EQUAL(sTar.size(), 4);
    BOOST_CHECK_EQUAL(sTar.get("tar").size(), 4);
    BOOST_CHECK_EQUAL(sTar.get("sub/name").size(), 0);
    BOOST_CHECK_THROW(sTar.get("not exist"), std::invalid_argument);

    // read with stream
    auto sFile   = sTar.get("hello");
    namespace io = boost::iostreams;
    io::stream<io::array_source> sStream(sFile.data(), sFile.size());
    std::string                  sBuffer;
    std::getline(sStream, sBuffer);
    BOOST_CHECK_EQUAL(sBuffer, "hello");
}
BOOST_AUTO_TEST_SUITE_END()