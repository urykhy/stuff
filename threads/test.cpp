#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <stdlib.h>
#include <iostream>
#include <Pipeline.hpp>
#include <Periodic.hpp> // for sleep

// g++ test.cpp -I. -lboost_unit_test_framework -pthread

BOOST_AUTO_TEST_SUITE(Threads)
BOOST_AUTO_TEST_CASE(pipeline)
{
    std::vector<int> data = {1,2,3,4,5,6,7,8,9};
    Threads::Pipeline<int> p;

    p.stage([](int a){
        std::cout<< "stage1 " << a << std::endl;
        Threads::sleep(drand48());
    });
    p.stage([](int a){
        std::cout<< "stage2 " << a << std::endl;
        Threads::sleep(drand48());
    });
    p.stage([](int a){
        std::cout<< "stage3 " << a << std::endl;
    });

    Threads::Group tg;
    p.start(tg, 3);
    sleep(1);

    for (auto a : data) {
        p.insert(a);
        Threads::sleep(0.1);
    }

    while (!p.idle()) Threads::sleep(0.1);

    tg.wait();  // call stop in Pipeline/SafeQueueThread
}
BOOST_AUTO_TEST_SUITE_END()
