#pragma once

#include "SafeQueue.hpp"

#include <container/Algorithm.hpp>
#include <unsorted/Ewma.hpp>

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
        static constexpr uint32_t IDLE_PERIOD          = 30;

        const uint32_t MAX_QUEUE_SIZE;

        mutable std::mutex m_Mutex;
        using Lock = std::unique_lock<std::mutex>;

        struct Node
        {
            std::string           user_id;
            std::function<void()> func;
        };

        using Q = SafeQueue<Node>;
        Q        m_Queue[Q_COUNT];
        uint32_t m_Budget[Q_COUNT]; // number of task we can perform from q until we can peek tasks from lower prio

        // user_id to queue_id
        std::unordered_map<std::string, uint32_t> m_UserQueue;

        struct Info
        {
            Util::Ewma ewma;
            double     spent = 0;
            uint32_t   idle  = 0;
        };
        std::unordered_map<std::string, Info> m_UserInfo;

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
                sTotal += x.pending();
            return sTotal;
        }

        // reset budgets to initial value
        void rewind()
        {
            memcpy(m_Budget, Q_WEIGHT, sizeof(m_Budget));
            refresh_i(time(nullptr));
        }

        std::pair<bool, uint32_t> get_queue_i()
        {
            while (true) {
                bool sIdle = true;

                for (uint32_t sQueueID = 0; sQueueID < Q_COUNT; sQueueID++) {
                    if (m_Budget[sQueueID] > 0 and !m_Queue[sQueueID].idle()) {
                        m_Budget[sQueueID]--;
                        return std::make_pair(false, sQueueID);
                    }
                    if (!m_Queue[sQueueID].idle())
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
            const auto sTask = m_Queue[sQueueID].try_get();
            if (!sTask)
                return false;
            aLock.unlock();

            const Time::Meter sMeter;
            sTask->func(); // must not throw
            const double sELA = sMeter.get().to_double();

            aLock.lock();
            m_UserInfo[sTask->user_id].spent += sELA;
            return false;
        }

        void refresh_i(time_t aNow)
        {
            if (m_LastSync >= aNow)
                return;

            // refresh users and get total weight
            double sTotal = 0;
            for (auto& [sUser, sInfo] : m_UserInfo) {
                sInfo.ewma.add(sInfo.spent);
                if (sInfo.spent > 0) {
                    sInfo.spent = 0;
                    sInfo.idle  = 0;
                } else {
                    sInfo.idle++;
                }
                sTotal += sInfo.ewma.estimate();
            }

            // drop idle
            Container::discard_if(m_UserInfo, [](auto& x) { return x.second.idle > IDLE_PERIOD; });

            // create new state map
            m_UserQueue = {};
            if (sTotal > 0 and m_UserInfo.size() > 1) {
                for (auto& [sUser, sInfo] : m_UserInfo) {
                    auto sRatio        = sInfo.ewma.estimate() / sTotal;
                    m_UserQueue[sUser] = ratioToQueueID(sRatio);
                }
            }

            m_LastSync = aNow;
        }

    public:
        FairQueueExecutor(uint32_t aQueueSize = 100)
        : MAX_QUEUE_SIZE(aQueueSize)
        {
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
            const bool sNotify = m_Queue[sQueueID].idle() and m_Wait;
            m_Queue[sQueueID].insert({aUserID, aFunc});
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
    };
} // namespace Threads