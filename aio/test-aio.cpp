/*
 * g++ test-aio.cpp -I. -lrt -pthread
 * AIO simple test
 */

#include <tut/tut.hpp>
#include <tut/tut_reporter.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <functional>

#include <NativeAio.hpp>
#include <PosixAio.hpp>
using namespace Util;
using namespace AIO;

namespace tut
{
    struct null_group {};
    typedef test_group<null_group> tg;
    typedef tg::object object;
    tg tg_1("aio test");

    template<>
    template<>
    void object::test<1>()
    {
        //ensure("true", true);
        int fd = open("/bin/bash", O_RDONLY | O_DIRECT, 0644);
        if (fd == -1) {
            throw "file not found";
        }

        NativeAio manager;
        manager.run();

        auto buf = manager.read(fd, 4096, 0);
        ensure("read rc", buf->get(std::chrono::seconds(1)) == 4096);
        // at least 1 page read's required

        ensure("byte1", 0x7f == buf->data(0));
        ensure("byte2", 'E' == buf->data(1));
        ensure("byte3", 'L' == buf->data(2));
        ensure("byte1", 'F' == buf->data(3));

        close(fd);
        manager.term();
        ensure("ops done", manager.pending() == 0);
    }

    template<>
    template<>
    void object::test<2>()
    {
        //ensure("true", true);
        int fd = open("/bin/bash", O_RDONLY, 0644);
        if (fd == -1) {
            throw "file not found";
        }

        PosixAio manager;
        char buf[4];

        manager.read(fd, buf, 4, 0);
        ensure("read rc", manager.wait(buf) == 4);
        // at least 1 page read's required

        ensure("byte1", 0x7f == buf[0]);
        ensure("byte2", 'E' == buf[1]);
        ensure("byte3", 'L' == buf[2]);
        ensure("byte1", 'F' == buf[3]);

        close(fd);
    }
} // namespace tut
int main()
{
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();
    return !reporter.all_ok();
}
