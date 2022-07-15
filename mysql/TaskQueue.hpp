#pragma once

#include <fmt/core.h>

#include <chrono>
#include <optional>

#include "Client.hpp"
#include "Quote.hpp"

#include <threads/Group.hpp>

namespace MySQL::TaskQueue {
    struct Config
    {
        std::string table  = "task_queue";
        std::string worker = "test";
        time_t      sleep  = 10;
        std::string resume = "status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)";
        std::string extra;

        bool isolation = false; // true to resume own tasks only
    };

    struct Task
    {
        uint64_t                   id = 0;
        std::string                task;
        std::string                worker;
        std::optional<std::string> cookie;

        Task(const MySQL::Row& aRow)
        : id(aRow[0])
        , task(aRow[1])
        , worker(aRow[2])
        {
            if (aRow[3])
                cookie = aRow[3];
        }
    };

    // data must be quoted
    using updateCookie = std::function<void(const std::string&)>;

    struct HandlerFace
    {
        virtual bool process(const Task& task, updateCookie&& api) = 0; // true = success
        virtual void report(const char* msg) noexcept              = 0; // report exceptions
        virtual ~HandlerFace() {}
    };

    class Manager
    {
        Config            m_Config;
        ConnectionFace*   m_Connection;
        HandlerFace*      m_Handler;
        std::atomic_bool  m_Exit{false};
        const std::string m_Where;

        std::string where() const
        {
            std::string sCond = "status = 'new'";

            if (!m_Config.resume.empty()) {
                std::string sResume = m_Config.resume;
                if (m_Config.isolation)
                    sResume.append(" AND worker = '" + m_Config.worker + "'");
                sCond += " OR (" + sResume + ")";
            }

            if (m_Config.extra.empty())
                return sCond;

            return m_Config.extra + " AND (" + sCond + ")";
        }

        std::optional<Task> get_task()
        {
            // SKIP LOCKED works on mysql 8
            std::optional<Task> sTask;

            m_Connection->ensure();
            m_Connection->Query("BEGIN"); // start transaction for `SELECT FOR UPDATE`

            m_Connection->Query(fmt::format(
                "SELECT id, task, worker, cookie "
                "FROM {0} "
                "WHERE {1} "
                "ORDER BY id ASC LIMIT 1 FOR UPDATE",
                m_Config.table,
                m_Where));
            m_Connection->Use([&sTask](const MySQL::Row& aRow) {
                sTask.emplace(aRow);
            });

            if (!sTask) {
                m_Connection->Query("COMMIT");
                return {};
            }

            m_Connection->Query(fmt::format(
                "UPDATE {0} "
                "SET status = 'started', worker = '{2}' "
                "WHERE id = {1}",
                m_Config.table,
                sTask->id,
                m_Config.worker));
            m_Connection->Query("COMMIT");

            return sTask;
        }

        std::string_view decode(bool aResult) const
        {
            return aResult ? "done" : "error";
        }

        void one_step_i()
        {
            auto sTask = get_task();
            if (!sTask) {
                wait(m_Config.sleep);
                return;
            }

            auto sStatusCode = m_Handler->process(*sTask, [this, &sTask](const std::string& aValue) {
                safe(fmt::format(
                    "UPDATE {0} "
                    "SET cookie = '{2}' "
                    "WHERE id = {1}",
                    m_Config.table,
                    sTask->id,
                    aValue));
            });

            safe(fmt::format(
                "UPDATE {0} "
                "SET status = '{2}' "
                "WHERE id = {1}",
                m_Config.table,
                sTask->id,
                decode(sStatusCode)));
        }

        void one_step()
        {
            try {
                one_step_i();
            } catch (const std::exception& e) {
                m_Connection->close();
                m_Handler->report(e.what());
                wait(m_Config.sleep); // do not flood with errors
            }
        }

        // retry query until success
        // throw if error and we must terminate
        void safe(const std::string& aQuery)
        {
            while (true) {
                try {
                    m_Connection->ensure();
                    m_Connection->Query(aQuery);
                    break;
                } catch (const std::exception& e) {
                    if (m_Exit)
                        throw;
                    m_Handler->report(e.what());
                    sleep(1);
                }
            }
        }

        void wait(time_t aTime)
        {
            using namespace std::chrono_literals;
            for (int i = 0; i < aTime * 10 and !m_Exit; i++)
                std::this_thread::sleep_for(100ms);
        }

    public:
        Manager(const Config& aConfig, ConnectionFace* aConnection, HandlerFace* aHandler)
        : m_Config(aConfig)
        , m_Connection(aConnection)
        , m_Handler(aHandler)
        , m_Where(where())
        {}

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this]() {
                while (!m_Exit)
                    one_step();
            });
            aGroup.at_stop([this]() {
                m_Exit = true;
            });
        }
    };
} // namespace MySQL::TaskQueue
