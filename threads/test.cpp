#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <stdlib.h>
#include <iostream>
#include <chrono>

#include "Pipeline.hpp"
#include "Periodic.hpp" // for sleep
#include "Collect.hpp"
#include "Asio.hpp"
#include "WaitGroup.hpp"
#include "OrderedWorker.hpp"
#include "MapReduce.hpp"
#include "ForEach.hpp"

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(Threads)
BOOST_AUTO_TEST_SUITE(pipeline)
BOOST_AUTO_TEST_CASE(sort)
{
    using PL = Threads::Pipeline<int>;
    PL::Stages stages;

    {
        PL::PriorityList list;
        list.push(PL::Node(1, stages.end(), 1, 1)); // serial 1, step 1
        list.push(PL::Node(2, stages.end(), 1, 2)); // serial 1, step 2
        list.push(PL::Node(3, stages.end(), 1, 3)); // serial 1, step 3
        BOOST_CHECK_EQUAL(list.top().data, 3);  // max step
    }
    {
        PL::PriorityList list;
        list.push(PL::Node(1, stages.end(), 1, 2)); // serial 1, step 2
        list.push(PL::Node(2, stages.end(), 2, 2)); // serial 2, step 2
        list.push(PL::Node(3, stages.end(), 3, 2)); // serial 3, step 2
        BOOST_CHECK_EQUAL(list.top().data, 1); // min serial
    }
}
BOOST_AUTO_TEST_CASE(impl)
{
    std::vector<int> data = {1,2,3,4,5,6,7,8,9};
    Threads::Pipeline<int> p;

    p.stage([](int a){
        BOOST_TEST_MESSAGE("stage1 " << a);
        Threads::sleep(drand48());
    });
    p.stage([](int a){
        BOOST_TEST_MESSAGE("stage2 " << a);
        Threads::sleep(drand48());
    });
    p.stage([](int a){
        BOOST_TEST_MESSAGE("stage3 " << a);
    });

    for (auto a : data)
        p.insert(a);

    Threads::Group tg;
    p.start(tg, 3);

    while (!p.idle()) Threads::sleep(0.1);
    tg.wait();  // call stop in Pipeline/SafeQueueThread
}
BOOST_AUTO_TEST_SUITE_END() // pipeline

BOOST_AUTO_TEST_CASE(wg)
{
    Threads::Group tg;
    Threads::Asio q;
    q.start(tg);

    int counter = 0;
    Threads::WaitGroup wg(10);
    for (int i = 0; i < 10; i++)
    {
        q.insert([&wg, &counter](){
            counter++;
            std::this_thread::sleep_for(50ms);
            wg.release();
        });
    }
    wg.wait();
    BOOST_CHECK_EQUAL(counter, 10);
}
BOOST_AUTO_TEST_CASE(ordered)
{
    struct Task {
        int duration;
        int serial;
    };

    Threads::OrderedWorker<Task> sWorker([](Task& a){
        BOOST_TEST_MESSAGE("perform task with serial " << a.serial);
        std::this_thread::sleep_for(std::chrono::milliseconds(a.duration));
        BOOST_TEST_MESSAGE("done task with serial " << a.serial);
    }, [last = 0](Task& a) mutable {
        BOOST_TEST_MESSAGE("join task with serial " << a.serial);
        BOOST_CHECK(a.serial > last); last++;
    });
    Threads::Group sGroup;
    sWorker.start(sGroup, 4);
    sWorker.insert(Task{400, 1});
    sWorker.insert(Task{200, 2});
    sWorker.insert(Task{50, 3});

    std::this_thread::sleep_for(500ms);
    sGroup.wait();
}
BOOST_AUTO_TEST_CASE(map_reduce)
{
    using C = Container::ListArray<unsigned>;
    C sList(100);
    for (unsigned i = 0; i <= 1000; i++)
        sList.push_back(i);

    unsigned sResult = Threads::MapReduce<unsigned>(sList, [](const auto t){
        unsigned sSum = 0;
        for (auto& x : t)
        {
            std::this_thread::sleep_for(1ms);
            sSum += x;
        }
        return sSum;
    }, [](auto& sSum, auto sData){
        sSum += sData;
    });
    BOOST_CHECK_EQUAL(500500, sResult);
}
BOOST_AUTO_TEST_CASE(for_each)
{
    std::list<unsigned> sList;
    for (unsigned i = 0; i <= 1000; i++)
        sList.push_back(i);

    std::atomic_uint32_t sResult = 0;
    Threads::ForEach(sList, [](unsigned a){ return a; }, [&sResult](unsigned a) mutable
    {
        std::this_thread::sleep_for(1ms);
        sResult += a;
    });
    BOOST_CHECK_EQUAL(500500, sResult);
}
BOOST_AUTO_TEST_SUITE_END() // Threads

BOOST_AUTO_TEST_SUITE(Asio)

// via: https://stackoverflow.com/questions/26694423/how-resume-the-execution-of-a-stackful-coroutine-in-the-context-of-its-strand/26728121#26728121
template <typename Signature, typename CompletionToken>
auto async_add_one(CompletionToken token, int value) {
    // Initialize the async completion handler and result
    // Careful to make sure token is a copy, as completion's handler takes a reference
    using completion_type = boost::asio::async_completion<CompletionToken, Signature>;
    completion_type completion{ token };

    BOOST_TEST_MESSAGE("spawning thread");
    std::thread([handler = completion.completion_handler, value]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        BOOST_TEST_MESSAGE("resume coroutine");
        // separate using statement is important
        // as asio_handler_invoke is overloaded based on handler's type
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(std::bind(handler, value + 1), &handler);
    }).detach();

    // Yield the coroutine.
    BOOST_TEST_MESSAGE("yield coroutine");
    return completion.result.get();
}
BOOST_AUTO_TEST_CASE(callback)
{
    Threads::Group tg;
    Threads::Asio q;
    q.start(tg);

    q.spawn([](boost::asio::yield_context yield)
    {
        BOOST_TEST_MESSAGE("started");
        const auto result1 = async_add_one<void(int)>(yield, 0);
        BOOST_CHECK_EQUAL(result1, 1);
        const auto result2 = async_add_one<void(int)>(yield, 41);
        BOOST_CHECK_EQUAL(result2, 42);
        BOOST_TEST_MESSAGE("finished");
    });
    sleep(3);
}
BOOST_AUTO_TEST_CASE(collect)
{
    Threads::Group tg;
    Threads::Asio q;
    q.start(tg);
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
BOOST_AUTO_TEST_SUITE_END() // Asio
