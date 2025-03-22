#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <fmt/chrono.h>
#include <fmt/core.h>
#include <stdlib.h>

#include <chrono>
#include <future>
#include <iostream>

#include <boost/asio/coroutine.hpp> // for pipeline::task

// for boost_cpp20 test
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

// for boost_generator test
#include <boost/asio/experimental/coro.hpp>

// for timer test
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>

// new timer test
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

// for cache adapter test
#include "Asio.hpp"
#include "Coro.hpp"
#include "FairQueueExecutor.hpp"
#include "ForEach.hpp"
#include "MapReduce.hpp"
#include "OrderedWorker.hpp"
#include "Periodic.hpp" // for sleep
#include "Pipeline.hpp"
#include "Spinlock.hpp"
#include "WaitGroup.hpp"

#include <cache/LRU.hpp>
#include <unsorted/Random.hpp>

using namespace std::chrono_literals;

auto sTimestamp = []() {
    const auto sNow   = std::chrono::system_clock::now();
    const auto sSince = sNow.time_since_epoch();
    const auto sUs    = std::chrono::duration_cast<std::chrono::microseconds>(sSince).count() % 1'000'000;
    return fmt::format("{:%Y-%m-%d %X}.{:06d}", sNow, sUs);
};

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
    BOOST_CHECK_CLOSE(sExecutor.debug().m_State["user-1"]->estimate().latency, 0.05, 5);
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

// boost + cpp20 coroutines
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;

awaitable<void> cpp20_step2()
{
    BOOST_TEST_MESSAGE("step2");
    co_return;
}

awaitable<void> cpp20_step1()
{
    BOOST_TEST_MESSAGE("step1 enter");
    co_await cpp20_step2();
    BOOST_TEST_MESSAGE("step1 exit");
}

BOOST_AUTO_TEST_CASE(boost_cpp20)
{
    boost::asio::io_context sContext(1);
    co_spawn(sContext, cpp20_step1(), detached);
    BOOST_TEST_MESSAGE("io context enter");
    sContext.run();
    BOOST_TEST_MESSAGE("io context exit");
}

boost::asio::experimental::generator<int> cpp20_gen2(boost::asio::any_io_executor exec)
{
    int i = 5;
    while (i >= 0)
        co_yield i--;
}

awaitable<void> cpp20_gen1()
{
    auto sGen = cpp20_gen2(co_await boost::asio::this_coro::executor);
    while (auto i = co_await sGen.async_resume(boost::asio::use_awaitable)) {
        BOOST_TEST_MESSAGE(*i);
    }
    co_return;
}

BOOST_AUTO_TEST_CASE(boost_generator)
{
    boost::asio::io_context sContext(1);
    co_spawn(sContext, cpp20_gen1(), detached);
    sContext.run();
}

awaitable<void> cpp20_timer(const std::string& aName)
{
    using namespace std::chrono_literals;
    using boost::asio::use_awaitable;
    using boost::asio::this_coro::executor;

    boost::asio::steady_timer sTimer(co_await executor);
    for (int i = 0; i < 5; i++) {
        BOOST_TEST_MESSAGE(aName << " " << i);
        sTimer.expires_from_now(100ms);
        co_await sTimer.async_wait(use_awaitable);
    }
}

BOOST_AUTO_TEST_CASE(boost_timer)
{
    boost::asio::io_context sContext(1);
    co_spawn(sContext, cpp20_timer("first"), detached);
    co_spawn(sContext, cpp20_timer("second"), detached);
    sContext.run();
}

// copy paste from
// https://stackoverflow.com/questions/76922347/use-error-codes-with-c20-coroutines-i-e-awaitables-with-boost-asio

boost::asio::awaitable<void> timer2(auto aDuration)
{
    namespace asio = boost::asio;
    auto [ec]      = co_await asio::steady_timer{
        co_await asio::this_coro::executor,
        aDuration}
                    .async_wait(as_tuple(asio::use_awaitable));
    BOOST_TEST_MESSAGE("Wait for " << (aDuration / 1.s) << "s: " << ec.message());
};

BOOST_AUTO_TEST_CASE(boost_timer2)
{
    namespace asio = boost::asio;
    using namespace std::chrono_literals;
    using namespace asio::experimental::awaitable_operators;

    asio::io_context sContext(1);
    asio::co_spawn(sContext, timer2(1s) || timer2(400ms), asio::detached);
    sContext.run();
}

BOOST_AUTO_TEST_CASE(boost_cancel_by_timer)
{
    namespace asio = boost::asio;
    using namespace std::chrono_literals;
    using namespace asio::experimental::awaitable_operators;

    bool             sFlag = false;
    asio::io_context sContext(1);

    auto f = [&]() -> boost::asio::awaitable<void> {
        auto sState = co_await boost::asio::this_coro::cancellation_state;
        while (sState.cancelled() == boost::asio::cancellation_type::none) {
            co_await timer2(30ms);
        }
        sFlag = true;
    };

    asio::co_spawn(sContext, (f() || timer2(100ms)), asio::detached);
    sContext.run_for(200ms);
    BOOST_CHECK_EQUAL(sFlag, true);
}

BOOST_AUTO_TEST_CASE(boost_cancel_with_exception)
{
    namespace asio = boost::asio;
    using namespace std::chrono_literals;
    using namespace asio::experimental::awaitable_operators;

    bool             sFlag = false;
    asio::io_context sContext(1);

    auto f = [&]() -> boost::asio::awaitable<void> {
        try {
            //co_await boost::asio::this_coro::throw_if_cancelled(true);
            while (true) {
                co_await timer2(30ms);
            }
        } catch (const std::exception& e) {
            sFlag = true;
            BOOST_TEST_MESSAGE("catch: " << e.what());
        }
    };

    asio::co_spawn(sContext, (f() || timer2(100ms)), asio::detached);
    sContext.run_for(200ms);
    BOOST_CHECK_EQUAL(sFlag, true);
}

BOOST_AUTO_TEST_CASE(co_await_multiple)
{
    namespace asio = boost::asio;
    using namespace std::chrono_literals;
    using namespace asio::experimental::awaitable_operators;

    auto f = []() -> boost::asio::awaitable<void> {
        boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
        sTimer.expires_from_now(100ms);
        BOOST_TEST_MESSAGE(sTimestamp() << " enter");
        co_await sTimer.async_wait(asio::use_awaitable);
        BOOST_TEST_MESSAGE(sTimestamp() << " leave");
    };

    asio::io_context sContext(1);
    asio::co_spawn(
        sContext, [&]() -> boost::asio::awaitable<void> {
            co_await (f() && f() && f());
        },
        asio::detached);
    sContext.run();
}

BOOST_AUTO_TEST_CASE(co_await_spawn_multiple)
{
    namespace asio = boost::asio;
    using namespace std::chrono_literals;
    using namespace asio::experimental::awaitable_operators;

    auto f = []() -> boost::asio::awaitable<void> {
        boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
        sTimer.expires_from_now(100ms);
        BOOST_TEST_MESSAGE(sTimestamp() << " enter");
        co_await sTimer.async_wait(asio::use_awaitable);
        BOOST_TEST_MESSAGE(sTimestamp() << " leave");
    };

    asio::io_context sContext(1);
    asio::co_spawn(
        sContext, [&]() -> boost::asio::awaitable<void> {
            co_await (asio::co_spawn(sContext, f, asio::use_awaitable) &&
                      asio::co_spawn(sContext, f, asio::use_awaitable) &&
                      asio::co_spawn(sContext, f, asio::use_awaitable));
        },
        asio::detached);
    sContext.run();
}

BOOST_AUTO_TEST_CASE(MapReduce)
{
    namespace asio = boost::asio;
    using namespace std::chrono_literals;

    std::vector<std::string>
        sInput{{"a"}, {"b"}, {"c"}, {"d"}, {"e"}};

    asio::io_context sContext(1);
    asio::co_spawn(
        sContext, [&]() -> boost::asio::awaitable<void> {
            auto sResult = co_await Threads::Coro::MapReduce<std::string>(
                sInput,
                [&](const auto& aData) -> asio::awaitable<std::string> {
                    boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                    sTimer.expires_from_now(100ms);
                    BOOST_TEST_MESSAGE(sTimestamp() << " enter " << aData);
                    co_await sTimer.async_wait(asio::use_awaitable);
                    BOOST_TEST_MESSAGE(sTimestamp() << " leave " << aData);
                    co_return aData;
                },
                [](auto& aResult, auto&& aTmp) -> asio::awaitable<void> {
                    boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                    sTimer.expires_from_now(10ms);
                    BOOST_TEST_MESSAGE(sTimestamp() << " reduce " << aTmp);
                    co_await sTimer.async_wait(asio::use_awaitable);
                    aResult.append(aTmp);
                    BOOST_TEST_MESSAGE(sTimestamp() << " reduced " << aTmp);
                    co_return;
                });
            BOOST_TEST_MESSAGE(sTimestamp() << " result: " << sResult);
            BOOST_CHECK_EQUAL(sResult, "abcde");
        },
        asio::detached);
    sContext.run();
}

BOOST_AUTO_TEST_SUITE(cache)
BOOST_AUTO_TEST_CASE(refresh)
{
    boost::asio::io_service                           sAsio;
    uint64_t                                          sNowMs        = 123;
    uint64_t                                          sRefreshCount = 0;
    Threads::Coro::CacheAdapter<int, int, Cache::LRU> sCache(
        1000 /* max size */,
        1000 /* deadline, ms */,
        [&sNowMs, &sRefreshCount](int a) mutable -> boost::asio::awaitable<std::pair<int, uint64_t>> {
            sRefreshCount++;
            using boost::asio::this_coro::executor;
            boost::asio::steady_timer sTimer(co_await executor);
            sTimer.expires_from_now(10ms);
            co_await sTimer.async_wait(boost::asio::use_awaitable);
            co_return std::pair(a, sNowMs);
        });

    uint64_t sSumTime = 0;
    for (int i = 0; i < 10; i++) {
        boost::asio::co_spawn(
            sAsio,
            [&]() -> boost::asio::awaitable<void> {
                Time::Meter sMeter;
                int         sValue = co_await sCache.Get(1, sNowMs);
                sSumTime += sMeter.get().to_ms();
                BOOST_CHECK_EQUAL(sValue, 1);
            },
            boost::asio::detached);
    }
    sAsio.run_for(500ms);
    BOOST_CHECK_EQUAL(sRefreshCount, 1);
    BOOST_TEST_MESSAGE("ela ms: " << sSumTime);
    BOOST_CHECK_GT(sSumTime, 98);
}
BOOST_AUTO_TEST_CASE(early_refresh)
{
    boost::asio::io_service                           sAsio;
    uint64_t                                          sNowMs        = 1000;
    uint64_t                                          sRefreshCount = 0;
    Threads::Coro::CacheAdapter<int, int, Cache::LRU> sCache(
        1000 /* max size */,
        1000 /* deadline, ms */,
        [&sNowMs, &sRefreshCount](int a) mutable -> boost::asio::awaitable<std::pair<int, uint64_t>> {
            sRefreshCount++;
            using boost::asio::this_coro::executor;
            boost::asio::steady_timer sTimer(co_await executor);
            sTimer.expires_from_now(10ms);
            co_await sTimer.async_wait(boost::asio::use_awaitable);
            co_return std::pair(a, sNowMs);
        });
    sCache.Put(1, 1, sNowMs);
    sNowMs += 950; // shift now to hit force early refresh

    uint64_t sSumTime = 0;
    for (int i = 0; i < 2; i++) {
        boost::asio::co_spawn(
            sAsio,
            [&]() mutable -> boost::asio::awaitable<void> {
                Time::Meter sMeter;
                int         sValue = co_await sCache.Get(1, sNowMs);
                sSumTime += sMeter.get().to_ms();
                BOOST_CHECK_EQUAL(sValue, 1);
            },
            boost::asio::detached);
    }
    sAsio.run_for(50ms);
    BOOST_CHECK_EQUAL(sRefreshCount, 1);
    BOOST_TEST_MESSAGE("ela ms: " << sSumTime);
    BOOST_CHECK_LE(sSumTime, 2);
}
BOOST_AUTO_TEST_SUITE_END() // cache
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

BOOST_AUTO_TEST_CASE(Atomic)
{
    using T   = std::map<std::string, std::string>;
    using Ptr = Threads::AtomicSharedPtr<T>;
    Ptr sPtr;
    sPtr.Update([](auto x) { x->operator[]("foo") = "bar"; });
    BOOST_CHECK_EQUAL(sPtr.Read()->operator[]("foo"), "bar");
}