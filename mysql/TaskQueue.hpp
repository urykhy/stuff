#pragma once

#include <chrono>
#include <optional>

#include "Client.hpp"
#include "Quote.hpp"
#include <threads/Group.hpp>

namespace MySQL::TaskQueue
{
    struct Config
    {
        MySQL::Config mysql;
        std::string   table = "task_queue";
        std::string   instance = "test";
        time_t        period = 10;
    };

    struct HandlerFace
    {
        virtual std::string prepare(const std::string& task) noexcept = 0; // return hint to resume task. can be empty string
        virtual bool        process(const std::string& task, const std::string& hint) = 0; // return true if success
        virtual void        report(const char* msg) noexcept = 0; // report exceptions
        virtual ~HandlerFace() {}
    };

    class Manager
    {
        Config       m_Config;
        Connection   m_Connection;
        HandlerFace* m_Handler;
        std::atomic_bool m_Exit{false};

        struct Task
        {
            uint64_t    id = 0;
            std::string task;
            std::string worker;
            std::string hint;
        };

        std::optional<Task> get_task()
        {
            std::optional<Task> sTask;
            const std::string sQuery = "SELECT id, task, worker, hint "
                                       "FROM " + m_Config.table + " "
                                       "WHERE (status = 'new') OR (status = 'started' and updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)) "
                                       "ORDER BY id ASC LIMIT 1 "
                                       "FOR UPDATE SKIP LOCKED";
            m_Connection.Query(sQuery);
            m_Connection.Use([&sTask](const MySQL::Row& aRow){
                sTask.emplace();
                sTask->id     = aRow.as_int(0);
                sTask->task   = aRow.as_str(1);
                sTask->worker = aRow.as_str(2);
                sTask->hint   = aRow.as_str(3);
            });
            return sTask;
        }

        void one_step_i()
        {
            m_Connection.ensure();
            m_Connection.Query("BEGIN");    // transaction required for `SELECT FOR UPDATE`
            std::optional<Task> sTask = get_task();
            if (!sTask)
            {
                m_Connection.Query("ROLLBACK");
                wait(m_Config.period);
                return;
            }

            std::string sUpdateHint;
            if (sTask->hint.empty())
            {
                sTask->hint = m_Handler->prepare(sTask->task);
                if (!sTask->hint.empty())
                    sUpdateHint = ", hint = '" + Quote(sTask->hint) + "' ";
            }
            m_Connection.Query("UPDATE " + m_Config.table + " SET status='started', worker='" + m_Config.instance + "'" + sUpdateHint + " WHERE id = " + std::to_string(sTask->id));
            m_Connection.Query("COMMIT");

            bool sStatusCode = m_Handler->process(sTask->task, sTask->hint);
            const std::string sStatusStr = sStatusCode ? "done" : "error";
            m_Connection.Query("UPDATE " + m_Config.table + " SET status='" + sStatusStr + "' WHERE id = " + std::to_string(sTask->id));
        }

        void one_step()
        {
            try
            {
                one_step_i();
            }
            catch (const std::exception& e)
            {
                m_Connection.close();
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
        Manager(const Config& aConfig, HandlerFace* aHandler)
        : m_Config(aConfig)
        , m_Connection(aConfig.mysql)
        , m_Handler(aHandler)
        {}

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this]()
            {
                while (!m_Exit)
                    one_step();
            });
            aGroup.at_stop([this](){
                m_Exit = true;
            });
        }
    };
}