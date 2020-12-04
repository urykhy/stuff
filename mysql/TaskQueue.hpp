#pragma once

#include <fmt/core.h>

#include <chrono>
#include <optional>

#include <threads/Group.hpp>

#include "Client.hpp"
#include "Quote.hpp"

namespace MySQL::TaskQueue {
    struct Config
    {
        std::string table  = "task_queue";
        std::string worker = "test";
        time_t      sleep  = 10;

        enum Isolation
        {
            STRICT, // process own tasks only (table.worker == config.worker)
            RESUME, // resume own tasks only
            NONE    // resume tasks by other worker
        };
        Isolation isolation = NONE;
    };

    struct Task
    {
        uint64_t    id = 0;
        std::string task;
        std::string worker;
        std::string cookie;
    };

    using updateCookieCB = std::function<void(const std::string&)>;

    struct HandlerFace
    {
        virtual bool process(const Task& task, updateCookieCB&& api) = 0; // return true if success
        virtual void report(const char* msg) noexcept                = 0; // report exceptions
        virtual ~HandlerFace() {}
    };

    class Manager
    {
        Config           m_Config;
        ConnectionFace*  m_Connection;
        HandlerFace*     m_Handler;
        std::atomic_bool m_Exit{false};

        std::string resume()
        {   // resume task only after a hour
            const std::string_view sCond = "(status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR))";
            switch (m_Config.isolation) {
            case Config::STRICT: return fmt::format("(status = 'new' OR {1}) AND worker = '{0}'", m_Config.worker, sCond);
            case Config::RESUME: return fmt::format("status = 'new' OR ({1} AND worker = '{0}')", m_Config.worker, sCond);
            case Config::NONE: return fmt::format("status = 'new' OR {0}", sCond);
            default: throw std::logic_error("TaskQueue::Manager::Resume");
            };
        }

        std::optional<Task> get_task()
        {
            std::optional<Task> sTask;
            m_Connection->Query(fmt::format(
                "SELECT id, task, worker, cookie "
                "FROM {0} "
                "WHERE {1} "
                "ORDER BY id ASC LIMIT 1 FOR UPDATE SKIP LOCKED",
                m_Config.table,
                resume()));
            m_Connection->Use([&sTask](const MySQL::Row& aRow) {
                sTask.emplace();
                sTask->id     = aRow[0].as_int64();
                sTask->task   = aRow[1].as_string();
                sTask->worker = aRow[2].as_string();
                sTask->cookie = aRow[3].as_string();
            });
            return sTask;
        }

        void one_step_i()
        {
            m_Connection->ensure();
            m_Connection->Query("BEGIN"); // start transaction for `SELECT FOR UPDATE`

            auto sTask = get_task();
            if (!sTask) {
                m_Connection->Query("ROLLBACK");
                wait(m_Config.sleep);
                return;
            }

            m_Connection->Query(fmt::format(
                "UPDATE {0} "
                "SET status = 'started', worker = '{2}' "
                "WHERE id = {1}",
                m_Config.table,
                sTask->id,
                m_Config.worker));
            m_Connection->Query("COMMIT");

            bool sStatusCode = m_Handler->process(*sTask, [this, &sTask](const std::string& aValue) {
                m_Connection->Query(fmt::format(
                    "UPDATE {0} "
                    "SET cookie = '{2}' "
                    "WHERE id = {1}",
                    m_Config.table,
                    sTask->id,
                    aValue));
            });

            const std::string sStatusStr = sStatusCode ? "done" : "error";
            m_Connection->Query(fmt::format(
                "UPDATE {0} "
                "SET status = '{2}' "
                "WHERE id = {1}",
                m_Config.table,
                sTask->id,
                sStatusStr));
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