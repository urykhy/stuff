#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <sys/sdt.h>

BOOST_AUTO_TEST_SUITE(sdt)
BOOST_AUTO_TEST_CASE(simple)
{
    DTRACE_PROBE(simple, enter);
    int reval = 0;
    DTRACE_PROBE1(simple, exit, reval);
}
BOOST_AUTO_TEST_SUITE_END()
