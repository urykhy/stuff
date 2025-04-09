#pragma once

#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <optional>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_service.hpp>

namespace Threads::Coro {

    // copy paste from https://www.scs.stanford.edu/~dm/blog/c++-coroutines.html
    struct Return
    {
        struct promise_type
        {
            std::exception_ptr exception_;

            Return get_return_object()
            {
                return {
                    .h_ = std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            std::suspend_never  initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void                unhandled_exception() { exception_ = std::current_exception(); }
            void                return_void() {}
        };

        std::coroutine_handle<promise_type> h_;
        operator std::coroutine_handle<promise_type>() const { return h_; }
        operator std::coroutine_handle<>() const { return h_; }
    };

    using Handle = std::coroutine_handle<Return::promise_type>;

    // awaitable Event for cpp20 coroutines + asio
    // based on https://kysa.me/boost-asio-coroutines-event/
    class Waiter
    {
        enum class State
        {
            IDLE,
            WAITING,
            READY
        };
        std::mutex                      m_Mutex;
        State                           m_State;
        std::move_only_function<void()> m_Handler;

    public:
        Waiter()
        : m_State{State::IDLE}
        {
        }

        boost::asio::awaitable<void> wait()
        {
            auto sExecutor = co_await boost::asio::this_coro::executor;
            auto sInitiate = [this, sExecutor]<typename Handler>(Handler&& aHandler) mutable {
                m_Handler = [sExecutor, aHandler = std::forward<Handler>(aHandler)]() mutable {
                    boost::asio::post(sExecutor, std::move(aHandler));
                };
                std::unique_lock sLock(m_Mutex);
                switch (m_State) {
                case State::IDLE:
                    m_State = State::WAITING;
                    break;
                case State::WAITING:
                    break;
                case State::READY:
                    sLock.unlock();
                    m_Handler();
                }
            };
            co_return co_await boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void()>(sInitiate, boost::asio::use_awaitable);
        }

        void notify()
        {
            std::unique_lock sLock(m_Mutex);
            switch (m_State) {
            case State::IDLE:
                m_State = State::READY;
                break;
            case State::WAITING:
                sLock.unlock();
                m_Handler();
                break;
            case State::READY:
                break;
            }
        }
    };

    template <class Key, class Value>
    class KeyUpdater
    {
    public:
        using ValueMs  = std::pair<Value, uint64_t>;
        using Handler  = std::function<boost::asio::awaitable<ValueMs>(Key)>;
        using Callback = std::function<void(const Key&, const Value&, uint64_t)>;

    private:
        struct WaitData
        {
            Key                  key{};
            std::optional<Value> value{};
            std::list<Waiter>    waiters{};
        };
        using WaitPtr = std::shared_ptr<WaitData>;
        using Waiters = std::map<Key, WaitPtr>;
        std::list<WaitPtr> m_List;
        Waiters            m_Waiters;
        Handler            m_Handler;
        Callback           m_Callback;

        boost::asio::awaitable<void> Coro()
        {
            while (!m_List.empty()) {
                co_await Refresh(m_List.front());
                m_List.pop_front();
            }
            co_return;
        }

        boost::asio::awaitable<void> Spawn(WaitPtr aPtr)
        {
            const bool sSpawn = m_List.empty();
            m_List.push_back(aPtr);
            if (sSpawn) {
                boost::asio::co_spawn(
                    co_await boost::asio::this_coro::executor,
                    [this]() -> boost::asio::awaitable<void> {
                        co_await Coro();
                    },
                    boost::asio::detached);
            }
        }

        boost::asio::awaitable<void> Refresh(WaitPtr aPtr)
        {
            uint64_t sNow               = 0;
            std::tie(aPtr->value, sNow) = co_await m_Handler(aPtr->key);
            m_Callback(aPtr->key, *aPtr->value, sNow);
            for (auto& x : aPtr->waiters) {
                x.notify();
            }
            m_Waiters.erase(aPtr->key);
            co_return;
        };

    public:
        KeyUpdater(Handler&& aHandler, Callback&& aCallback)
        : m_Handler(std::move(aHandler))
        , m_Callback(std::move(aCallback))
        {
        }

        boost::asio::awaitable<Value> Wait(const Key& aKey)
        {
            auto sPtr      = co_await Schedule(aKey);

            // check if value already available
            if (sPtr->value) {
                co_return *sPtr->value;
            }

            // no value, must wait
            sPtr->waiters.emplace_back();
            auto sWaiter = --sPtr->waiters.end();
            co_await sWaiter->wait();

            // got response and cleanup
            sPtr->waiters.erase(sWaiter);
            co_return *sPtr->value;
        }

        boost::asio::awaitable<WaitPtr> Schedule(const Key& aKey)
        {
            auto& sPtr = m_Waiters[aKey];
            if (!sPtr) {
                sPtr = std::make_shared<WaitData>(WaitData{.key = aKey});
                co_await Spawn(sPtr);
            }
            co_return sPtr;
        }
    };

    template <class Key, class Value, template <class, class> class Cache>
    class CacheAdapter
    {
        struct Entry
        {
            uint64_t created_at{0}; // ms
            bool     early_refresh{false};
            Value    value = {};
        };
        Cache<Key, Entry>      m_Cache;
        const uint64_t         m_Deadline; // ms
        KeyUpdater<Key, Value> m_Updater;

    public:
        CacheAdapter(size_t aMaxSize, uint64_t aDeadline, KeyUpdater<Key, Value>::Handler&& aRefresh)
        : m_Cache(aMaxSize)
        , m_Deadline(aDeadline)
        , m_Updater(std::move(aRefresh),
                    [this](const Key& aKey, const Value& aValue, const uint64_t aNow) {
                        Put(aKey, aValue, aNow);
                    })
        {
        }

        boost::asio::awaitable<Value> Get(const Key& aKey, const uint64_t aNow)
        {
            auto sPtr = m_Cache.Get(aKey);
            if (sPtr == nullptr) { // no data
                co_return co_await m_Updater.Wait(aKey);
            }
            if (sPtr->created_at + m_Deadline <= aNow) { // expired
                co_return co_await m_Updater.Wait(aKey);
            }
            if (sPtr->created_at + m_Deadline * 0.9 <= aNow and !sPtr->early_refresh) { // almost expired
                const_cast<Entry*>(sPtr)->early_refresh = true;
                co_await m_Updater.Schedule(aKey);
            }
            co_return sPtr->value;
        }

        void Put(const Key& aKey, const Value& aValue, const uint64_t aNow)
        {
            m_Cache.Put(aKey, Entry{.created_at    = aNow,
                                    .early_refresh = false,
                                    .value         = aValue});
        }
    };

    template <class Result, class D, class M, class R>
    boost::asio::awaitable<Result> MapReduce(const D& aData, M aMapper, R aReducer, unsigned aMax = 4)
    {
        Result sResult;
        Waiter sWaiter;
        size_t sCounter  = 0;
        auto   sExecutor = co_await boost::asio::this_coro::executor;
        using MapResult  = typename std::invoke_result_t<M, typename D::value_type>::value_type;
        std::list<MapResult> sReduceQueue;

        auto sReducer = [&]() -> boost::asio::awaitable<void> {
            while (!sReduceQueue.empty()) {
                co_await aReducer(sResult, sReduceQueue.front());
                sCounter++;
                sReduceQueue.pop_front();
            }
            if (sCounter == aData.size()) {
                sWaiter.notify();
            }
        };
        auto sIt      = aData.begin();
        auto sHandler = [&]() -> boost::asio::awaitable<void> {
            while (sIt != aData.end()) {
                auto sLocalIt = sIt++;
                auto sTmp     = co_await aMapper(*sLocalIt);
                bool sSpawn   = sReduceQueue.empty();
                sReduceQueue.push_back(std::move(sTmp));
                if (sSpawn) {
                    co_spawn(sExecutor, sReducer, boost::asio::detached);
                }
            }
        };

        using Task = decltype(boost::asio::co_spawn(
            sExecutor,
            sHandler,
            boost::asio::deferred));
        std::list<Task> sTasks;

        for (unsigned i = 0; i < aMax; i++) {
            sTasks.push_back(
                boost::asio::co_spawn(
                    sExecutor,
                    sHandler,
                    boost::asio::deferred));
        }
        auto sGroup = boost::asio::experimental::make_parallel_group(std::move(sTasks));
        co_await sGroup.async_wait(boost::asio::experimental::wait_for_all(), boost::asio::use_awaitable);
        co_await sWaiter.wait();
        co_return sResult;
    }

} // namespace Threads::Coro
