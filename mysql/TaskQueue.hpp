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
        std::string table    = "task_queue";
        std::string instance = "test";
        time_t      period   = 10;
        bool        resume   = true; // resume tasks by other worker

        unsigned shard_count = 0; // sharding
        unsigned shard_id    = 0;

        unsigned window = 0; // max lag between started tasks
    };

    struct HandlerFace
    {
        virtual std::string prepare(const std::string& task) noexcept                 = 0; // return hint to resume task. can be empty string
        virtual bool        process(const std::string& task, const std::string& hint) = 0; // return true if success
        virtual void        report(const char* msg) noexcept                          = 0; // report exceptions
        virtual ~HandlerFace() {}
    };

    class Manager
    {
        Config           m_Config;
        ConnectionFace*  m_Connection;
        HandlerFace*     m_Handler;
        std::atomic_bool m_Exit{false};

        struct Task
        {
            uint64_t    id = 0;
            std::string task;
            std::string worker;
            std::string hint;
        };

        std::string shard()
        {
            if (m_Config.shard_count > 0)
                return "id % " + std::to_string(m_Config.shard_count) + " = " + std::to_string(m_Config.shard_id) + " AND";
            return "";
        }

        std::string resume()
        {
            // resume only self tasks
            if (!m_Config.resume)
                return "worker = '" + m_Config.instance + "'";

            // resume task by other worker
            return "(worker = '" + m_Config.instance + "' OR updated < DATE_SUB(NOW(), INTERVAL 1 HOUR))";
        }

        std::optional<Task> get_task()
        {
            std::optional<Task> sTask;
            m_Connection->Query(fmt::format(
                "SELECT id, task, worker, hint "
                "FROM {0} "
                "WHERE {1} ((status = 'new') OR (status = 'started' AND {2})) "
                "ORDER BY id ASC LIMIT 1 FOR UPDATE SKIP LOCKED",
                m_Config.table,
                shard(),
                resume()));
            m_Connection->Use([&sTask](const MySQL::Row& aRow) {
                sTask.emplace();
                sTask->id     = aRow[0].as_int64();
                sTask->task   = aRow[1].as_string();
                sTask->worker = aRow[2].as_string();
                sTask->hint   = aRow[3].as_string();
            });
            if (sTask and m_Config.window > 0) {
                m_Connection->Query(fmt::format(
                    "SELECT count(1) "
                    "FROM {0} "
                    "WHERE {1} id < {2} AND id >= (SELECT min(id) FROM {0} WHERE {1} status='started')",
                    m_Config.table,
                    shard(),
                    sTask->id));
                unsigned sCount = 0;
                m_Connection->Use([&sCount](const MySQL::Row& aRow) {
                    sCount = aRow[0].as_int64();
                });
                if (sCount > m_Config.window)
                    sTask = std::nullopt;
            }
            return sTask;
        }

        void one_step_i()
        {
            m_Connection->ensure();
            m_Connection->Query("BEGIN"); // start transaction for `SELECT FOR UPDATE`
            std::optional<Task> sTask = get_task();
            if (!sTask) {
                m_Connection->Query("ROLLBACK");
                wait(m_Config.period);
                return;
            }

            std::string sUpdateHint;
            if (sTask->hint.empty()) {
                sTask->hint = m_Handler->prepare(sTask->task);
                if (!sTask->hint.empty())
                    sUpdateHint = ", hint = '" + Quote(sTask->hint) + "' ";
            }
            m_Connection->Query(fmt::format(
                "UPDATE {0} "
                "SET status = 'started', worker = '{1}' {2}"
                "WHERE id = {3}",
                m_Config.table,
                m_Config.instance,
                sUpdateHint,
                sTask->id));
            m_Connection->Query("COMMIT");

            bool              sStatusCode = m_Handler->process(sTask->task, sTask->hint);
            const std::string sStatusStr  = sStatusCode ? "done" : "error";

            m_Connection->Query(fmt::format(
                "UPDATE {0} "
                "SET status = '{1}' "
                "WHERE id = {2}",
                m_Config.table,
                sStatusStr,
                sTask->id));
        }

        void one_step()
        {
            try {
                one_step_i();
            } catch (const std::exception& e) {
                m_Connection->close();
                m_Handler->report(e.what());
                wait(1); // do not flood with errors
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