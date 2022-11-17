#pragma once

#include <array>

#include "SafeQueue.hpp"

#include <container/Algorithm.hpp>
#include <unsorted/Ewma.hpp>

#ifdef BOOST_TEST_MODULE
#include <fmt/core.h>
#endif

namespace Threads {

    // inspired by FairCallQueue / hdfs
    class FairQueueExecutor
    {
#ifdef BOOST_TEST_MODULE
    public:
#endif
        static constexpr uint32_t Q_COUNT              = 4;
        static constexpr double   Q_THRES[Q_COUNT - 1] = {0.05, 0.15, 0.45};
        static constexpr uint32_t Q_WEIGHT[Q_COUNT]    = {8, 4, 2, 1};
        static constexpr uint32_t MAX_WEIGHT_FACTOR    = 1000;
        static constexpr uint32_t IDLE_PERIOD          = 30;
        static constexpr double   MIN_TIME             = 0.0001;
        static constexpr double   MAX_WAIT             = 2;

        const uint32_t MAX_QUEUE_SIZE;

        mutable std::mutex m_Mutex;
        using Lock = std::unique_lock<std::mutex>;

        struct Node
        {
            std::string           user_id;
            std::function<void()> func;
            Time::time_spec       insert_at;
        };

        struct QInfo
        {
            SafeQueue<Node> queue;
            uint32_t        budget   = 0;
            uint32_t        estimate = 0;
            Util::Ewma      avg_response_time;
            double          sum_respone_time = 0;
            Util::Ewma      avg_call_time;
            double          sum_call_time = 0;
            uint32_t        calls         = 0;
            uint64_t        total_calls   = 0;
        };
        std::array<QInfo, Q_COUNT> m_Queue;

        // user_id to queue_id
        std::unordered_map<std::string, uint32_t> m_UserQueue;

        struct UserInfo
        {
            Util::Ewma avg_time;
            double     sum_time = 0;
            uint32_t   idle     = 0;
        };
        std::unordered_map<std::string, UserInfo> m_UserInfo;

        time_t           m_LastSync = 0;
        std::atomic_bool m_Stop     = false;
        bool             m_Wait     = false;

        // to wait new task, if idle
        std::condition_variable m_Cond;

        uint32_t ratioToQueueID(double aRatio)
        {
            for (uint32_t i = 0; i < Q_COUNT - 1; i++)
                if (aRatio < Q_THRES[i])
                    return i;
            return Q_COUNT - 1;
        }

        uint32_t pending_i() const
        {
            uint32_t sTotal = 0;
            for (auto& x : m_Queue)
                sTotal += x.queue.pending();
            return sTotal;
        }

        void init()
        {
            for (uint32_t i = 0; i < Q_COUNT; i++) {
                m_Queue[i].estimate = Q_WEIGHT[i];
            }
        }

        // reset budgets to initial value
        void rewind()
        {
            refresh_i(time(nullptr));
            for (auto& x : m_Queue)
                x.budget = x.estimate;
        }

        std::pair<bool, uint32_t> get_queue_i()
        {
            while (true) {
                bool sIdle = true;

                for (uint32_t sQueueID = 0; sQueueID < Q_COUNT; sQueueID++) {
                    if (m_Queue[sQueueID].budget > 0 and !m_Queue[sQueueID].queue.idle()) {
                        m_Queue[sQueueID].budget--;
                        return std::make_pair(false, sQueueID);
                    }
                    if (!m_Queue[sQueueID].queue.idle())
                        sIdle = false;
                }

                if (sIdle)
                    return std::make_pair(true, 0);
                else
                    rewind();
            }
        }

        bool one_step(Lock& aLock)
        {
            const auto [sIdle, sQueueID] = get_queue_i();
            if (sIdle) {
                return sIdle;
            }
            const auto sTask = m_Queue[sQueueID].queue.try_get();
            if (!sTask)
                return false;
            aLock.unlock();

            const Time::Meter sMeter;
            sTask->func(); // must not throw
            const double sELA          = sMeter.get().to_double();
            const double sResponseTime = (Time::get_time() - sTask->insert_at).to_double();

            aLock.lock();
            auto& sInfo = m_UserInfo[sTask->user_id];
            sInfo.sum_time += sELA;
            m_Queue[sQueueID].sum_call_time += sELA;
            m_Queue[sQueueID].sum_respone_time += sResponseTime;
            m_Queue[sQueueID].calls++;
            return false;
        }

        void refresh_i_user_queue()
        {
            // refresh users avg_time and get total time
            double sTotal = 0;
            for (auto& [sUser, sInfo] : m_UserInfo) {
                sInfo.avg_time.add(sInfo.sum_time);
                if (sInfo.sum_time > 0) {
                    sInfo.sum_time = 0;
                    sInfo.idle     = 0;
                } else {
                    sInfo.idle++;
                }
                sTotal += sInfo.avg_time.estimate();
            }

            // drop idle
            Container::discard_if(m_UserInfo, [](auto& x) { return x.second.idle > IDLE_PERIOD; });

            // create new state map
            m_UserQueue.clear();
            if (sTotal > 0 and m_UserInfo.size() > 1) {
                for (auto& [sUser, sInfo] : m_UserInfo) {
                    const auto sRatio   = sInfo.avg_time.estimate() / sTotal;
                    const auto sQueueID = ratioToQueueID(sRatio);
                    // BOOST_TEST_MESSAGE("assign user " << sUser << " with ratio " << sRatio << " to queue-" << sQueueID);
                    m_UserQueue[sUser] = sQueueID;
                }
            }
        }

        void refresh_i_queue_avg_time()
        {
            // estimate avg time to make single call
            // refresh avg response time
            for (uint32_t i = 0; i < Q_COUNT; i++) {
                if (m_Queue[i].calls > 0) {
                    m_Queue[i].avg_call_time.add(m_Queue[i].sum_call_time / m_Queue[i].calls);
                    m_Queue[i].avg_response_time.add(m_Queue[i].sum_respone_time / m_Queue[i].calls);
                    m_Queue[i].total_calls += m_Queue[i].calls;
                    m_Queue[i].sum_call_time    = 0;
                    m_Queue[i].sum_respone_time = 0;
                    m_Queue[i].calls            = 0;
                } else {
                    m_Queue[i].avg_call_time.add(0);
                    m_Queue[i].avg_response_time.add(0);
                }
            }
        }

        void refresh_i_queue_budget()
        {
            double sStepTime = 0;
            for (uint32_t i = 0; i < Q_COUNT; i++) {
                const uint32_t sQueueID     = Q_COUNT - (i + 1);
                const double   sAvgCallTime = std::max(m_Queue[sQueueID].avg_call_time.estimate(), MIN_TIME);
                m_Queue[sQueueID].estimate  = Q_WEIGHT[sQueueID];
                if (sQueueID == Q_COUNT - 1) {
                    sStepTime = std::max(sAvgCallTime, MIN_TIME);
                } else {
                    sStepTime *= (Q_WEIGHT[sQueueID] / Q_WEIGHT[sQueueID + 1]);
                    if (sAvgCallTime > MIN_TIME and sQueueID < Q_COUNT) {
                        // BOOST_TEST_MESSAGE("queue[" << sQueueID << "]: step time " << sStepTime << ", call time: " << sAvgCallTime);
                        const uint32_t sCalls      = std::round(sStepTime / sAvgCallTime);
                        m_Queue[sQueueID].estimate = std::clamp(sCalls, Q_WEIGHT[sQueueID], Q_WEIGHT[sQueueID] * MAX_WEIGHT_FACTOR);
                        // BOOST_TEST_MESSAGE("queue[" << sQueueID << "]: weight " << m_Queue[sQueueID].estimate);
                    }
                }
            }
        }

        void refresh_i(time_t aNow)
        {
            if (m_LastSync >= aNow)
                return;

            refresh_i_user_queue();
            refresh_i_queue_avg_time();
            refresh_i_queue_budget();

            m_LastSync = aNow;
        }

    public:
        FairQueueExecutor(uint32_t aQueueSize = 100)
        : MAX_QUEUE_SIZE(aQueueSize)
        {
            init();
            rewind();
        }

        void start(Group& aGroup, unsigned aCount = 1)
        {
            aGroup.start(
                [this]() {
                    while (!m_Stop) {
                        Lock sLock(m_Mutex);
                        if (one_step(sLock)) {
                            m_Wait = true;
                            m_Cond.wait_for(sLock, std::chrono::milliseconds(500));
                            refresh_i(time(nullptr));
                            m_Wait = false;
                        }
                    }
                },
                aCount);
            aGroup.at_stop([this]() { m_Stop = true; });
        }

        // return false if task not accepted (queue size limit)
        bool insert(const std::string& aUserID, std::function<void()> aFunc)
        {
            Lock sLock(m_Mutex);
            if (pending_i() > MAX_QUEUE_SIZE)
                return false;

            uint32_t sQueueID = 0;
            if (auto sIt = m_UserQueue.find(aUserID); sIt != m_UserQueue.end()) {
                sQueueID = sIt->second;
            }
            // backoff by response time
            if (m_Queue[sQueueID].avg_response_time.estimate() > MAX_WAIT)
                return false;

            const bool sNotify = m_Queue[sQueueID].queue.idle() and m_Wait;
            m_Queue[sQueueID].queue.insert({aUserID, aFunc, Time::get_time()});
            if (sNotify) {
                m_Cond.notify_one();
                m_Wait = false;
            }
            return true;
        }

        bool idle() const
        {
            Lock sLock(m_Mutex);
            return m_Wait;
        }

        // recalculate user bucket
        void refresh(time_t aNow)
        {
            Lock sLock(m_Mutex);
            refresh_i(aNow);
        }

        // queue size
        uint32_t pending() const
        {
            Lock sLock(m_Mutex);
            return pending_i();
        }

#ifdef BOOST_TEST_MODULE
        void debug() const
        {
            Lock sLock(m_Mutex);
            for (uint32_t i = 0; i < Q_COUNT; i++) {
                BOOST_TEST_MESSAGE(fmt::format(
                    "queue[{}]: calls: {:4}, weight: {:3}, call time: {:.5f}, response time: {:.5f}",
                    i,
                    m_Queue[i].total_calls,
                    m_Queue[i].estimate,
                    m_Queue[i].avg_call_time.estimate(),
                    m_Queue[i].avg_response_time.estimate()));
            };
            for (auto& [sUser, sQueueID] : m_UserQueue) {
                auto sIt = m_UserInfo.find(sUser);
                if (sIt == m_UserInfo.end())
                    continue;
                BOOST_TEST_MESSAGE(fmt::format(
                    "user {}: queue-{}, avg time used {:.5f}",
                    sUser,
                    sQueueID,
                    sIt->second.avg_time.estimate()));
            }
        }
#endif
    };
} // namespace Threads