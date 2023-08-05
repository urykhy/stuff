#pragma once

#include <array>

#include "SafeQueue.hpp"

#include <unsorted/Ewma.hpp>

#ifdef BOOST_TEST_MODULE
#include <fmt/core.h>
#endif

namespace Threads::Fair {

    using State = std::shared_ptr<Util::EwmaRps>;

    template <class T>
    struct Task
    {
        T           task;
        std::string user;
        State       state = {};
#ifdef BOOST_TEST_MODULE
        time_t now      = 0;
        double duration = 0;
#endif
    };

    template <class T>
    class List
    {
#ifdef BOOST_TEST_MODULE
    public:
#endif
        std::multimap<uint64_t, T>             m_List;
        std::unordered_map<std::string, State> m_State;

        double prepare(T& aTask)
        {
            const double sWeight = Time::get_time().to_double();
            double       sTmp    = 0;
            auto&        sState  = m_State[aTask.user];
            if (sState) {
                const auto sEst = sState->estimate();
                sTmp            = sEst.latency * sEst.rps * (m_List.size() + 1);
            } else {
                sState = std::make_shared<Util::EwmaRps>();
            };
            aTask.state = sState;
            return sWeight + sTmp;
        }

    public:
        void push(T&& aTask)
        {
            auto sWeight = prepare(aTask);
            m_List.emplace(sWeight, std::move(aTask));
        }
        void push(const T& aTask)
        {
            auto sTmp    = aTask;
            auto sWeight = prepare(sTmp);
            m_List.emplace(sWeight, std::move(sTmp));
        }
        bool   empty() const { return m_List.empty(); }
        T&     top() { return m_List.begin()->second; }
        void   pop() { m_List.erase(m_List.begin()); }
        size_t size() const { return m_List.size(); }
    };

    // Task must kind of Task<T>
    template <class Task>
    class QueueThread : public SafeQueueThread<Task, List<Task>>
    {
        using Base = SafeQueueThread<Task, List<Task>>;
        const std::function<void(Task& t)> m_Handler;

#ifdef BOOST_TEST_MODULE
    public:
#endif
        void handler(Task&& aTask)
        {
            auto   sState = aTask.state;
            time_t sNow   = 0;
#ifdef BOOST_TEST_MODULE
            const double sDuration = aTask.duration;
            sNow                   = aTask.now;
#endif
            const Time::Meter sMeter;

            auto sAccounting = [&](bool aSuccess) {
                double sElapsed = sMeter.get().to_double();
#ifdef BOOST_TEST_MODULE
                if (sDuration > 0)
                    sElapsed = sDuration;
#endif
                sState->add(sElapsed, sNow > 0 ? sNow : time(nullptr), aSuccess);
            };

            try {
                m_Handler(aTask);
                sAccounting(true);
            } catch (...) {
                sAccounting(false);
                throw;
            }
        }

    public:
        template <class F>
        QueueThread(F aHandler)
        : Base([this](auto& aTask) { handler(std::move(aTask)); })
        , m_Handler(aHandler)
        {
        }
    };

    struct QueueExecutor : public QueueThread<Task<std::function<void()>>>
    {
        using T = Task<std::function<void()>>;
        using Base = QueueThread<T>;

        QueueExecutor()
        : Base([](auto& aTask) { aTask.task(); })
        {
        }

        void insert(std::function<void()>&& aFunc, const std::string& aUser)
        {
            Base::insert(T{.task = std::move(aFunc), .user = aUser});
        }
    };
} // namespace Threads::Fair