#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <stdlib.h>
#include <iostream>
#include <Pipeline.hpp>
#include <Periodic.hpp> // for sleep
#include <Collect.hpp>
#include <WorkQ.hpp>

// g++ test.cpp -I. -I.. -lboost_system -lboost_unit_test_framework -pthread

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
BOOST_AUTO_TEST_CASE(delay)
{
    int iter = 0;
    Threads::DelayQueueThread<std::string> t([&iter](auto& x){
        switch (iter)
        {
            case 0: BOOST_CHECK_EQUAL(x, "test3"); break;
            case 1: BOOST_CHECK_EQUAL(x, "test2"); break;
            case 2: BOOST_CHECK_EQUAL(x, "test1"); break;
            default: throw "unexpected call";
        }
        iter++;
    });
    Threads::Group tg;
    t.start(tg);
    t.insert(2, "test1");
    t.insert(1, "test2");
    t.insert(0, "test3");
    while (!t.idle()) Threads::sleep(0.1);
    tg.wait();
}
BOOST_AUTO_TEST_CASE(collect)
{
    Threads::Group tg;
    Threads::WorkQ q;
    q.start(1, tg);
    Threads::Collect c(q.service(), [](std::string&& a){
        BOOST_CHECK_EQUAL(a, "123asdiop");
    });

    std::vector<std::string> test_data({"123","asd","iop"});
    for (const auto& x : test_data)
    {
        c.push(x);
        Threads::sleep(0.1);
    }
    while (!c.empty())
        Threads::sleep(0.1);
    tg.wait();
}
BOOST_AUTO_TEST_SUITE_END()
