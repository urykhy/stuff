#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <stdlib.h>

#include <chrono>
#include <iostream>

#include <boost/asio/coroutine.hpp> // for pipeline::task

#include "Asio.hpp"
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
BOOST_AUTO_TEST_CASE(fair_queue_1)
{
    Threads::FairQueueExecutor sFair;
    Util::Ewma                 sEwma;

    // prepare state
    // user-1 used 0.1 second
    sFair.m_UserInfo["user-1"] = Threads::FairQueueExecutor::UserInfo{
        .avg_time = sEwma,
        .sum_time = 0.1,
    };
    // user-2 used 10 seconds
    sFair.m_UserInfo["user-2"] = Threads::FairQueueExecutor::UserInfo{
        .avg_time = sEwma,
        .sum_time = 10,
    };
    sFair.refresh(time(nullptr) + 10); // disable auto refreshes for a while

    BOOST_CHECK_EQUAL(sFair.m_UserQueue["user-1"], 0);
    BOOST_CHECK_EQUAL(sFair.m_UserQueue["user-2"], 3);

    sFair.refresh(time(nullptr) + 11); // refresh and log budgets

    // push tasks
    std::list<std::string> sOrder;
    sFair.insert("user-2", [&sOrder]() { sOrder.push_back("user-2"); });
    for (int i = 0; i < 10; i++)
        sFair.insert("user-1", [&sOrder]() { sOrder.push_back("user-1"); });

    // process tasks
    while (true) {
        Threads::FairQueueExecutor::Lock sLock(sFair.m_Mutex);
        if (sFair.one_step(sLock))
            break;
    }

    // check order
    std::list<std::string> sExpected{
        "user-1", "user-1", "user-1", "user-1", "user-1", "user-1", "user-1", "user-1", "user-2", "user-1", "user-1"};
    BOOST_CHECK_EQUAL_COLLECTIONS(sOrder.begin(), sOrder.end(), sExpected.begin(), sExpected.end());
}

struct FairTest
{
    static constexpr int       COUNT = 2000;
    Threads::FairQueueExecutor m_Fair;
    Threads::Group             m_Group;

    FairTest()
    : m_Fair(COUNT)
    {
    }

    template <class P, class G>
    void operator()(P aPrepare, G aGen)
    {
        aPrepare(m_Fair);
        m_Fair.start(m_Group, 6);

        std::atomic<size_t> sCount = 0;
        for (int i = 0; i < COUNT; i++) {
            std::string sUser = std::string("user-") + aGen();
            while (true) {
                const bool sOk = m_Fair.insert(sUser, [&sCount]() {
                    sCount++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (sOk)
                    break;
            }

            if (i % 50 == 0)
                m_Fair.refresh(time(nullptr) + i);
        }

        // force refresh assotiations
        for (int i = 0; i < 20 and !m_Fair.idle(); i++) {
            m_Fair.refresh(time(nullptr) + COUNT + i);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        m_Group.wait();
        m_Fair.debug();
        BOOST_CHECK_EQUAL(sCount, COUNT);
    }
};

BOOST_AUTO_TEST_CASE(fair_queue_2_disbalance)
{
    // no initial state (in balance)
    auto sPrepare = [](auto& aFair) {};

    // ~ time share by users: (disbalance)
    //  4 % - user-0
    // 10 % - user-1
    // 86 % - user-2
    auto sGen = []() {
        double sRnd = Util::drand48();
        if (sRnd < 0.04)
            return "0";
        else if (sRnd < 0.14)
            return "1";
        else
            return "2";
    };

    FairTest sTest;
    sTest(sPrepare, sGen);

    BOOST_CHECK_EQUAL(sTest.m_Fair.m_UserQueue["user-0"], 0);
    BOOST_CHECK_EQUAL(sTest.m_Fair.m_UserQueue["user-1"], 1);
    BOOST_CHECK_EQUAL(sTest.m_Fair.m_UserQueue["user-2"], 3);
}
BOOST_AUTO_TEST_CASE(fair_queue_3_rebalance)
{
    // prepare user/queue mapping (disbalance)
    auto sPrepare = [](auto& aFair) {
        aFair.m_UserQueue["user-0"] = 0;
        aFair.m_UserQueue["user-1"] = 1;
        aFair.m_UserQueue["user-2"] = 3;

        Util::Ewma sEwma;
        aFair.m_UserInfo["user-0"] = Threads::FairQueueExecutor::UserInfo{
            .avg_time = sEwma,
            .sum_time = 0.1,
        };
        aFair.m_UserInfo["user-1"] = Threads::FairQueueExecutor::UserInfo{
            .avg_time = sEwma,
            .sum_time = 0.2,
        };
        aFair.m_UserInfo["user-2"] = Threads::FairQueueExecutor::UserInfo{
            .avg_time = sEwma,
            .sum_time = 1,
        };
    };

    // ~ time share by users:
    // 1/3 (in balance)
    auto sGen = []() {
        double sRnd = Util::drand48();
        if (sRnd < 0.33)
            return "0";
        else if (sRnd < 0.66)
            return "1";
        else
            return "2";
    };

    FairTest sTest;
    sTest(sPrepare, sGen);

    BOOST_CHECK_EQUAL(sTest.m_Fair.m_UserQueue["user-0"], 2);
    BOOST_CHECK_EQUAL(sTest.m_Fair.m_UserQueue["user-1"], 2);
    BOOST_CHECK_EQUAL(sTest.m_Fair.m_UserQueue["user-2"], 2);
}
BOOST_AUTO_TEST_CASE(fair_queue_4_backoff)
{
    Threads::FairQueueExecutor sFair;

    sFair.m_Queue[2].avg_response_time.reset(0.5);
    sFair.m_Queue[3].avg_response_time.reset(1);
    sFair.m_UserQueue["user-1"] = 2; // push user 1 to q-2
    sFair.m_UserQueue["user-2"] = 3; // push user 2 to q-3

    BOOST_CHECK_EQUAL(true, sFair.insert("user-1", []() {}));
    BOOST_CHECK_EQUAL(true, sFair.insert("user-2", []() {}));

    sFair.m_Queue[3].avg_response_time.reset(3); // FairQueueExecutor::MAX_WAIT = 2
    BOOST_CHECK_EQUAL(false, sFair.insert("user-2", []() {}));
    BOOST_CHECK_EQUAL(true, sFair.insert("user-0", []() {}));
}
BOOST_AUTO_TEST_CASE(fair_queue_5_qw)
{
    Threads::FairQueueExecutor sFair;

    sFair.m_Queue[0].avg_call_time.reset(0.1);
    sFair.m_Queue[1].avg_call_time.reset(0.5);
    sFair.m_Queue[2].avg_call_time.reset(1);
    sFair.m_Queue[3].avg_call_time.reset(2);
    sFair.refresh_i_queue_budget();

    BOOST_CHECK_EQUAL(160, sFair.m_Queue[0].estimate);
    BOOST_CHECK_EQUAL(16, sFair.m_Queue[1].estimate);
    BOOST_CHECK_EQUAL(4, sFair.m_Queue[2].estimate);
    BOOST_CHECK_EQUAL(1, sFair.m_Queue[3].estimate);

    // total time:
    // 0.1 * 160 + 0.5 * 16 + 1 * 4 + 2 = 30
    // time share for queue-0:
    // 0.1 * 160 / 30 = ~ 8/15
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
