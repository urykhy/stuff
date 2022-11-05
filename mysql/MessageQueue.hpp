#pragma once

#include <fmt/core.h>

#include <string>
#include <vector>

#include "Client.hpp"

namespace MySQL::MessageQueue {

    struct Producer
    {
        struct Config
        {
            std::string queue = "events:0";
        };

    private:
        const Config    m_Config;
        ConnectionFace* m_Connection;

    public:
        enum Status
        {
            OK,
            ALREADY
        };

        Producer(const Config& aConfig, ConnectionFace* aConnection)
        : m_Config(aConfig)
        , m_Connection(aConnection)
        {
        }

        bool is_exists(std::string_view aTask)
        {
            bool sExists = false;

            const std::string sQuery = fmt::format(
                "SELECT 1"
                "  FROM mq_data"
                " WHERE queue='{}' AND task='{}'",
                m_Config.queue,
                aTask);
            m_Connection->Query(sQuery);
            m_Connection->Use([&sExists](const MySQL::Row& aRow) { sExists = true; });

            return sExists;
        }

        // user must start and commit transaction
        Status insert(std::string_view aTask)
        {
            uint64_t sPosition = 0;

            std::string sQuery = fmt::format(
                "SELECT position"
                "  FROM mq_producer"
                " WHERE queue='{}'"
                "   FOR UPDATE",
                m_Config.queue);
            m_Connection->Query(sQuery);
            m_Connection->Use([&sPosition](const MySQL::Row& aRow) { sPosition = aRow[0]; });

            if (is_exists(aTask)) {
                return ALREADY;
            }

            sPosition++;

            sQuery = fmt::format(
                "INSERT INTO mq_data(queue, serial, task) "
                "VALUES ('{}', {}, '{}')",
                m_Config.queue,
                sPosition,
                aTask);
            m_Connection->Query(sQuery);

            sQuery = fmt::format(
                "INSERT INTO mq_producer(queue, position) "
                "VALUES ('{}',{}) "
                "ON DUPLICATE KEY UPDATE position=VALUES(position)",
                m_Config.queue,
                sPosition);
            m_Connection->Query(sQuery);
            return OK;
        }
    };

    struct Consumer
    {
        struct Config
        {
            std::string name  = "service:0";
            std::string queue = "events:0";
            unsigned    limit = 10;
        };

    private:
        const Config    m_Config;
        ConnectionFace* m_Connection;
        uint64_t        m_Position = 0;

        void init()
        {
            const std::string sQuery = fmt::format(
                "SELECT position"
                "  FROM mq_consumer"
                " WHERE name='{}' AND queue='{}'",
                m_Config.name,
                m_Config.queue);
            m_Connection->Query(sQuery);
            m_Connection->Use([this](const MySQL::Row& aRow) { m_Position = aRow[0]; });
        }

    public:
        struct Task
        {
            uint64_t    serial = 0;
            std::string task;

            auto as_tuple() const { return std::tie(serial, task); }
            bool operator==(const Task& aOther) const { return as_tuple() == aOther.as_tuple(); }
        };
        using List = std::vector<Task>;

        Consumer(const Config& aConfig, ConnectionFace* aConnection)
        : m_Config(aConfig)
        , m_Connection(aConnection)
        {
            init();
        }

        List select()
        {
            List sTasks;

            const std::string sQuery = fmt::format(
                "SELECT serial, task"
                "  FROM mq_data"
                " WHERE queue = '{}' AND serial > {}"
                " ORDER BY serial ASC"
                " LIMIT {}",
                m_Config.queue,
                m_Position,
                m_Config.limit);
            m_Connection->Query(sQuery);
            m_Connection->Use([&sTasks](const MySQL::Row& aRow) {
                Task sTask;
                sTask.serial = aRow[0];
                sTask.task   = aRow[1];
                sTasks.push_back(std::move(sTask));
            });

            if (!sTasks.empty())
                m_Position = sTasks.back().serial;

            return sTasks;
        }

        // user must start and commit transaction
        void update()
        {
            const std::string sQuery = fmt::format(
                "INSERT INTO mq_consumer(name, queue, position) "
                "VALUES ('{}', '{}', {}) "
                "ON DUPLICATE KEY UPDATE position=VALUES(position)",
                m_Config.name,
                m_Config.queue,
                m_Position);
            m_Connection->Query(sQuery);
        }
    };
} // namespace MySQL::MessageQueue
