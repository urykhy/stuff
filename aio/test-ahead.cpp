// nonblock idea from https://lkml.org/lkml/2007/5/30/400

/*
 * g++ test-ahead.cpp
 * strace -ttt -T -f ./a.out
 * echo 3 > /proc/sys/vm/drop_caches
 * OLD:  splice/reading file can now return EAGAIN
 * 4.17: no EAGAIN
 *
 */

#include <iostream>
#include <vector>
#include <cassert>

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

void sleep()
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    nanosleep(&ts, NULL);
}

int main(void)
{
    int pp[2];
    int rc = pipe2(pp, O_NONBLOCK);
    assert (rc != -1);

    int zero = open("/dev/null", O_WRONLY);
    assert(zero != -1);

    std::string fname = "/home/ury/tmp/largefile";
    int fd = open(fname.c_str(), O_RDONLY|O_NONBLOCK);
    assert(fd != -1);

    struct stat st;
    rc = fstat(fd, &st);
    assert(rc == 0);

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    rc = readahead(fd, 0, 4096);
    assert (rc == 0);
    sleep();

    const size_t len = 4096 * 16;
    const size_t max = st.st_size;
    off64_t count = 0;
    while (count < max)
    {
        size_t tail = (max - count > len) ? len : (max - count);
        //std::cout << "count = " << count << ", max = " << max << ", tail = " << tail << std::endl;
        rc = splice(fd, NULL, pp[1], NULL, tail, SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
        //std::cerr << "splice1 rc " << rc << "/" << errno<< std::endl;
        if (-1 == rc && errno == EAGAIN)
        {
            sleep();
            continue;
        }
        assert (rc > 0);
        count += rc;
        rc = splice(pp[0], NULL, zero, NULL, rc, SPLICE_F_MOVE);
        assert(rc > 0);
    };

    return 0;
}
