#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Native.hpp"

BOOST_AUTO_TEST_SUITE(AIO)
BOOST_AUTO_TEST_CASE(native)
{
    //ensure("true", true);
    int fd = open("/bin/bash", O_RDONLY | O_DIRECT, 0644);
    if (fd == -1) {
        throw "file not found";
    }

    AIO::Native sManager;
    Threads::Group sGroup;
    sManager.start(sGroup);

    // at least 1 page read's required
    sManager.read(fd, 4096, 0, [](int rc, AIO::BufferPtr buf)
    {
        const char* data = static_cast<const char*>(buf->data());
        BOOST_CHECK_EQUAL(4096, rc);
        BOOST_CHECK_EQUAL(0x7f, data[0]);
        BOOST_CHECK_EQUAL( 'E', data[1]);
        BOOST_CHECK_EQUAL( 'L', data[2]);
        BOOST_CHECK_EQUAL( 'F', data[3]);
    });
    std::this_thread::sleep_for(100ms);

    close(fd);
}
BOOST_AUTO_TEST_SUITE_END()
