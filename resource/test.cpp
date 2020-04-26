#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Get.hpp"
#include "Tar.hpp"

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>

DECLARE_RESOURCE(sample_data_data)
DECLARE_RESOURCE(sample_tar_tar)

BOOST_AUTO_TEST_SUITE(resource)
BOOST_AUTO_TEST_CASE(simple)
{   // objcopy --input binary --output elf64-x86-64 --binary-architecture i386:x86-64 sample.data sample.o
    const auto sTmp = resource::sample_data_data();
    BOOST_TEST_MESSAGE(sTmp);
    BOOST_CHECK_EQUAL(sTmp, "some data\nto inject\ninto elf\n");
}
BOOST_AUTO_TEST_CASE(tar)
{
    resource::Tar sTar(resource::sample_tar_tar());
    BOOST_CHECK_EQUAL(sTar.size(), 4);
    BOOST_CHECK_EQUAL(sTar.get("tar").size(), 4);
    BOOST_CHECK_EQUAL(sTar.get("sub/name").size(), 0);
    BOOST_CHECK_THROW(sTar.get("not exist"), std::runtime_error);

    // read with stream
    auto sFile = sTar.get("hello");
    namespace io = boost::iostreams;
    io::stream<io::array_source> sStream(sFile.data(), sFile.size());
    std::string sBuffer;
    std::getline(sStream, sBuffer);
    BOOST_CHECK_EQUAL(sBuffer, "hello");
}
BOOST_AUTO_TEST_SUITE_END()