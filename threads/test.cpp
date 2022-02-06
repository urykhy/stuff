#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <stdlib.h>

#include <chrono>
#include <iostream>

#include <boost/asio/coroutine.hpp> // for pipeline::task

#include "Asio.hpp"
#include "ForEach.hpp"
#include "MapReduce.hpp"
#include "OrderedWorker.hpp"
#include "Periodic.hpp" // for sleep
#include "Pipeline.hpp"
#include "WaitGroup.hpp"

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(pipeline)
BOOST_AUTO_TEST_CASE(simple)
{
#include <boost/asio/yield.hpp>
    struct Task : public Threads::Pipeline::Task, public boost::asio::coroutine
    {
        const uint64_t m_ID;
        Task(uint64_t aID)
        : m_ID(aID)
        {}

        virtual void operator()(Wrap&& aWrap)
        {
            reenter(this)
            {
                BOOST_TEST_MESSAGE("task " << m_ID << ": enter");
                yield aWrap([this, counter = 0]() mutable {
                    BOOST_TEST_MESSAGE("task " << m_ID << ": make step 1");
                    counter++;
                    if (m_ID == 2 and counter < 3)
                        throw std::runtime_error("task error");
                });
                BOOST_TEST_MESSAGE("task " << m_ID << ": after step 1");
                yield aWrap([this]() { BOOST_TEST_MESSAGE("task " << m_ID << ": make step 2"); });
                BOOST_TEST_MESSAGE("task " << m_ID << ": after step 2");
                BOOST_TEST_MESSAGE("task " << m_ID << ": leave");
            }
        }
    };
#include <boost/asio/unyield.hpp>

    Threads::Pipeline::Manager sManager;
    sManager.insert(std::make_shared<Task>(1));
    sManager.insert(std::make_shared<Task>(2));
    sManager.insert(std::make_shared<Task>(3));

    Threads::Group tg;
    sManager.start(tg);

    while (!sManager.idle())
        Threads::sleep(0.1);

    tg.wait(); // call stop in Pipeline/SafeQueueThread
}
BOOST_AUTO_TEST_SUITE_END() // pipeline

BOOST_AUTO_TEST_SUITE(Threads)
BOOST_AUTO_TEST_CASE(wg)
{
    Threads::Group tg;
    Threads::Asio  q;
    q.start(tg);

    int                counter = 0;
    Threads::WaitGroup wg(10);
    for (int i = 0; i < 10; i++) {
        q.insert([&wg, &counter]() {
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
    struct Task
    {
        int duration;
        int serial;
    };

    Threads::OrderedWorker<Task> sWorker([](Task& a) {
        BOOST_TEST_MESSAGE("perform task with serial " << a.serial);
        std::this_thread::sleep_for(std::chrono::milliseconds(a.duration));
        BOOST_TEST_MESSAGE("done task with serial " << a.serial); }, [last = 0](Task& a) mutable {
        BOOST_TEST_MESSAGE("join task with serial " << a.serial);
        BOOST_CHECK(a.serial > last); last++; });
    Threads::Group               sGroup;
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

    unsigned sResult = Threads::MapReduce<unsigned>(
        sList, [](const auto t) {
        unsigned sSum = 0;
        for (auto& x : t)
        {
            std::this_thread::sleep_for(1ms);
            sSum += x;
        }
        return sSum; }, [](auto& sSum, auto sData) { sSum += sData; });
    BOOST_CHECK_EQUAL(500500, sResult);
}
BOOST_AUTO_TEST_CASE(for_each)
{
    std::list<unsigned> sList;
    for (unsigned i = 0; i <= 1000; i++)
        sList.push_back(i);

    std::atomic_uint32_t sResult = 0;
    Threads::ForEach(
        sList, [](unsigned a) { return a; }, [&sResult](unsigned a) mutable {
        std::this_thread::sleep_for(1ms);
        sResult += a; });
    BOOST_CHECK_EQUAL(500500, sResult);
}
BOOST_AUTO_TEST_SUITE_END() // Threads

BOOST_AUTO_TEST_SUITE(Asio)

// via: https://stackoverflow.com/questions/26694423/how-resume-the-execution-of-a-stackful-coroutine-in-the-context-of-its-strand/26728121#26728121
template <typename Signature, typename CompletionToken>
auto async_add_one(CompletionToken token, int value)
{
    // Initialize the async completion handler and result
    // Careful to make sure token is a copy, as completion's handler takes a reference
    using completion_type = boost::asio::async_completion<CompletionToken, Signature>;
    completion_type completion{token};

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
    Threads::Asio  q;
    q.start(tg);

    q.spawn([](boost::asio::yield_context yield) {
        BOOST_TEST_MESSAGE("started");
        const auto result1 = async_add_one<void(int)>(yield, 0);
        BOOST_CHECK_EQUAL(result1, 1);
        const auto result2 = async_add_one<void(int)>(yield, 41);
        BOOST_CHECK_EQUAL(result2, 42);
        BOOST_TEST_MESSAGE("finished");
    });
    sleep(3);
}
BOOST_AUTO_TEST_SUITE_END() // Asio
