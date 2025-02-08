#pragma once

#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <optional>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_service.hpp>

#include <unsorted/Raii.hpp>

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
        State                           m_State;
        std::move_only_function<void()> m_Handler;

    public:
        Waiter()
        : m_State{State::IDLE}
        {
        }

        boost::asio::awaitable<void> wait(boost::asio::any_io_executor executor)
        {
            auto initiate = [this, executor]<typename Handler>(Handler&& handler) mutable {
                m_Handler = [executor, handler = std::forward<Handler>(handler)]() mutable {
                    boost::asio::post(executor, std::move(handler));
                };
                switch (m_State) {
                case State::IDLE:
                    m_State = State::WAITING;
                    break;
                case State::WAITING:
                    break;
                case State::READY:
                    m_Handler();
                }
            };
            return boost::asio::async_initiate<decltype(boost::asio::use_awaitable), void()>(initiate, boost::asio::use_awaitable);
        }

        void notify()
        {
            switch (m_State) {
            case State::IDLE:
                m_State = State::READY;
                break;
            case State::WAITING:
                m_Handler();
                break;
            case State::READY:
                break;
            }
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
        Cache<Key, Entry> m_Cache;

        struct WaitData
        {
            std::list<Waiter>    waiters{};
            std::optional<Value> value{};
            bool                 done{false};
        };
        using Waiters = std::map<Key, WaitData>;
        Waiters m_Waiters;

        const uint64_t m_Deadline; // ms
        using RefreshT = std::function<boost::asio::awaitable<std::pair<Value, uint64_t>>(Key)>;
        RefreshT m_Refresh;

        void Cleanup(Waiters::iterator aIt)
        {
            if (aIt->second.done and aIt->second.waiters.empty()) {
                m_Waiters.erase(aIt);
            }
        }

        boost::asio::awaitable<void> Refresh(const Key& aKey, Waiters::iterator aIt)
        {
            uint64_t sNow                     = 0;
            std::tie(aIt->second.value, sNow) = co_await m_Refresh(aKey);
            Put(aKey, *aIt->second.value, sNow); // store new value
            for (auto& x : aIt->second.waiters) {
                x.notify();
            }
            aIt->second.done = true;
            co_return;
            // FIXME: check for exception
        };

        boost::asio::awaitable<typename Waiters::iterator> SpawnRefresh(const Key& aKey)
        {
            auto sExecutor = co_await boost::asio::this_coro::executor;
            auto sIt       = m_Waiters.find(aKey);
            if (sIt == m_Waiters.end()) {
                bool sAlready;
                std::tie(sIt, sAlready) = m_Waiters.emplace(aKey, WaitData{});
                boost::asio::co_spawn(
                    sExecutor,
                    [this, aKey, sIt]() -> boost::asio::awaitable<void> {
                        co_await Refresh(aKey, sIt);
                    },
                    boost::asio::detached);
            }
            co_return sIt;
        }

        boost::asio::awaitable<Value> Wait(const Key& aKey)
        {
            auto sExecutor = co_await boost::asio::this_coro::executor;
            auto sIt       = co_await SpawnRefresh(aKey);

            Util::Raii cleanup([this, sIt]() { Cleanup(sIt); });

            // check if value already available
            if (sIt->second.value) {
                co_return *sIt->second.value;
            }

            // no value, must wait
            sIt->second.waiters.push_back({});
            auto sCurrent = --sIt->second.waiters.end();
            co_await sCurrent->wait(sExecutor);

            // got response and cleanup
            auto sValue = *sIt->second.value;
            sIt->second.waiters.erase(sCurrent);
            co_return sValue;
        }

    public:
        CacheAdapter(size_t aMaxSize, uint64_t aDeadline, RefreshT&& aRefresh)
        : m_Cache(aMaxSize)
        , m_Deadline(aDeadline)
        , m_Refresh(std::move(aRefresh))
        {
        }

        boost::asio::awaitable<Value> Get(const Key& aKey, const uint64_t aNow)
        {
            auto sPtr = m_Cache.Get(aKey);
            if (sPtr == nullptr) { // no data
                co_return co_await Wait(aKey);
            }
            if (sPtr->created_at + m_Deadline <= aNow) { // expired
                co_return co_await Wait(aKey);
            }
            if (sPtr->created_at + m_Deadline * 0.9 <= aNow and !sPtr->early_refresh) { // almost expired
                const_cast<Entry*>(sPtr)->early_refresh = true;
                co_await SpawnRefresh(aKey);
            }
            co_return sPtr->value;
        }

        void Put(const Key& aKey, const Value& aValue, const uint64_t aNow)
        {
            m_Cache.Put(aKey, Entry{.created_at    = aNow,
                                    .early_refresh = false,
                                    .value         = aValue});
        }

        // FIXME: cleanup possible stale entries in m_Waiters
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
        co_await sWaiter.wait(sExecutor);
        co_return sResult;
    }

} // namespace Threads::Coro
