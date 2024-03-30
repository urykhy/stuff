#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <stdlib.h>

#include <chrono>
#include <future>
#include <iostream>

#include <boost/asio/coroutine.hpp> // for pipeline::task

#include "Asio.hpp"
#include "Coro.hpp"
#include "FairQueueExecutor.hpp"
#include "ForEach.hpp"
#include "MapReduce.hpp"
#include "OrderedWorker.hpp"
#include "Periodic.hpp" // for sleep
#include "Pipeline.hpp"
#include "WaitGroup.hpp"

#include <unsorted/Random.hpp>

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
        {
        }

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
BOOST_AUTO_TEST_CASE(cancel)
{
    struct A
    {
        A() { BOOST_TEST_MESSAGE("A constructed"); }
        ~A() { BOOST_TEST_MESSAGE("A destroyed"); }
    };
    std::thread sTmp([]() {
        A a;
        BOOST_TEST_MESSAGE("start sleep");
        try {
            sleep(1);
        } catch (const abi::__forced_unwind&) {
            BOOST_CHECK_MESSAGE(true, "got exception");
            throw;
        }
        BOOST_CHECK_MESSAGE(false, "no exception");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pthread_cancel(sTmp.native_handle());
    sTmp.join();
}
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
BOOST_AUTO_TEST_CASE(fair_test1)
{
    using Task = Threads::Fair::Task<std::string>;
    using E    = Threads::Fair::QueueThread<Task>;

    E              sExecutor([](Task& aTask) {
        BOOST_TEST_MESSAGE("handle task " << aTask.task << " as user " << aTask.user);
        Threads::sleep(0.1);
    });
    Threads::Group sGroup;
    sExecutor.start(sGroup);

    sExecutor.insert(Task{.task = "1", .user = "user-1", .now = 1});
    sExecutor.insert(Task{.task = "2", .user = "user-1", .now = 2});
    sExecutor.insert(Task{.task = "3", .user = "user-1", .now = 3});

    sExecutor.wait(5);
    sGroup.wait();
    BOOST_CHECK_CLOSE(sExecutor.debug().m_State["user-1"]->estimate().latency, 0.01, 5);
}
BOOST_AUTO_TEST_CASE(fair_test2)
{
    using Task = Threads::Fair::Task<std::string>;
    using E    = Threads::Fair::QueueThread<Task>;

    E              sExecutor([](Task& aTask) {
        BOOST_TEST_MESSAGE("handle task " << aTask.task << " as user " << aTask.user);
    });
    Threads::Group sGroup;

    // init state
    auto& sState     = sExecutor.debug().m_State;
    sState["user-1"] = std::make_shared<Util::EwmaRps>();
    sState["user-1"]->reset({.latency = 0.1, .rps = 1});

    sState["user-2"] = std::make_shared<Util::EwmaRps>();
    sState["user-2"]->reset({.latency = 0.2, .rps = 1});

    sState["user-3"] = std::make_shared<Util::EwmaRps>();
    sState["user-3"]->reset({.latency = 0.5, .rps = 1});

    for (int j = 1; j < 20; j++)
        for (int i = 1; i < 4; i++)
            sExecutor.insert(Task{.task = std::to_string(j), .user = "user-" + std::to_string(i), .now = j * i, .duration = 0.2});

    sExecutor.start(sGroup);
    sExecutor.wait(5);
    sGroup.wait();

    for (auto& [sUser, sState] : sState) {
        const auto sEst = sState->estimate();
        BOOST_TEST_MESSAGE(fmt::format(
            "user {}: latency {:.2f}, rps {:.2f}",
            sUser,
            sEst.latency,
            sEst.rps));
    }
}
BOOST_AUTO_TEST_SUITE_END() // Threads

BOOST_AUTO_TEST_SUITE(Asio)

template <typename Token>
auto async_add_one(Token token, int value)
{
    auto init = [value](auto handler) {
        BOOST_TEST_MESSAGE("coroutine: spawn thread");
        std::thread([handler = std::move(handler), value]() mutable {
            boost::system::error_code ec;
            BOOST_TEST_MESSAGE("thread: sleep");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            BOOST_TEST_MESSAGE("thread: resume coroutine");
            handler(ec, value + 1);
        }).detach();
    };
    BOOST_TEST_MESSAGE("coroutine: wait for result");
    return boost::asio::async_initiate<Token, void(boost::system::error_code, int)>(init, token);
}
BOOST_AUTO_TEST_CASE(callback)
{
    Threads::Group tg;
    Threads::Asio  q;
    q.start(tg);

    q.spawn([](boost::asio::yield_context yield) {
        BOOST_TEST_MESSAGE("started");
        const auto result1 = async_add_one(yield, 0);
        BOOST_CHECK_EQUAL(result1, 1);
        const auto result2 = async_add_one(yield, 41);
        BOOST_CHECK_EQUAL(result2, 42);
        BOOST_TEST_MESSAGE("finished");
    });
    sleep(3);
}
BOOST_AUTO_TEST_SUITE_END() // Asio

BOOST_AUTO_TEST_SUITE(Coro)
Threads::Coro::Return step2()
{
    for (unsigned i = 0; i < 3; ++i) {
        co_await std::suspend_always{};
        BOOST_TEST_MESSAGE("step2: " << i);
    }
}
Threads::Coro::Return step1()
{
    for (unsigned i = 0; i < 3; ++i) {
        co_await std::suspend_always{};
        BOOST_TEST_MESSAGE("step1: " << i);
    }

    Threads::Coro::Handle h = step2();
    while (!h.done()) {
        co_await std::suspend_always{};
        h();
    }
    h.destroy();
}
BOOST_AUTO_TEST_CASE(basic)
{
    Threads::Coro::Handle h       = step1();
    auto&                 promise = h.promise();
    while (!h.done()) {
        BOOST_TEST_MESSAGE("in main function");
        h();
    }
    if (promise.exception_ != nullptr)
        BOOST_TEST_MESSAGE("exit with exception");
    h.destroy();
}
BOOST_AUTO_TEST_SUITE_END() // Coro

BOOST_AUTO_TEST_SUITE(Async)
BOOST_AUTO_TEST_CASE(basic)
{
    // step 1.
    auto sFuture = std::async(std::launch::async, []() -> int {
        sleep(2);
        return 1;
    });
    sleep(1);
    BOOST_CHECK_EQUAL(sFuture.valid(), true);
    while (sFuture.wait_for(0us) != std::future_status::ready) {
        Threads::sleep(0.1);
        BOOST_TEST_MESSAGE(".");
    }
    BOOST_CHECK_EQUAL(sFuture.valid(), true);
    BOOST_CHECK(sFuture.get() == 1);
    BOOST_CHECK_EQUAL(sFuture.valid(), false);

    // step 2
    sFuture = std::async(std::launch::async, []() -> int {
        sleep(2);
        return 2;
    });
    BOOST_CHECK_EQUAL(sFuture.valid(), true);
    BOOST_CHECK(sFuture.get() == 2);
}
BOOST_AUTO_TEST_SUITE_END()
