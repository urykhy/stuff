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
            std::string producer = "producer:0";
        };

    private:
        const Config    m_Config;
        ConnectionFace* m_Connection;

    public:
        enum Status
        {
            OK,
            ALREADY,
            CONFLICT
        };

        Producer(const Config& aConfig, ConnectionFace* aConnection)
        : m_Config(aConfig)
        , m_Connection(aConnection)
        {}

        bool is_exists(std::string_view aTask)
        {
            bool sExists = false;

            const std::string sQuery = fmt::format(
                "SELECT 1"
                "  FROM message_queue"
                " WHERE producer='{}' AND task='{}'",
                m_Config.producer,
                aTask);
            m_Connection->Query(sQuery);
            m_Connection->Use([&sExists](const MySQL::Row& aRow) { sExists = true; });

            return sExists;
        }

        // user must start and commit transaction
        Status insert(std::string_view aTask, std::string_view aHash = {})
        {
            m_Connection->ensure();
            try {
                uint64_t sPosition = 0;

                std::string sQuery = fmt::format(
                    "SELECT position"
                    "  FROM message_state"
                    " WHERE service='{}'"
                    "   FOR UPDATE",
                    m_Config.producer);
                m_Connection->Query(sQuery);
                m_Connection->Use([&sPosition](const MySQL::Row& aRow) { sPosition = aRow[0].as_uint64(); });

                if (is_exists(aTask)) {
                    return ALREADY;
                }

                sPosition++;

                sQuery = fmt::format(
                    "INSERT INTO message_queue(producer, serial, task, hash) "
                    "VALUES ('{}', {}, '{}', '{}')",
                    m_Config.producer,
                    sPosition,
                    aTask,
                    aHash);
                m_Connection->Query(sQuery);

                sQuery = fmt::format(
                    "INSERT INTO message_state(service, position) "
                    "VALUES ('{}',{}) "
                    "ON DUPLICATE KEY UPDATE position=VALUES(position)",
                    m_Config.producer,
                    sPosition);
                m_Connection->Query(sQuery);
                return OK;
            } catch (const Connection::Error& aErr) {
                if (aErr.m_Errno == ER_DUP_ENTRY or aErr.m_Errno == ER_LOCK_WAIT_TIMEOUT) {
                    return Status::CONFLICT;
                }
                throw;
            } catch (...) {
                throw;
            }
        }
    };

    struct Consumer
    {
        struct Config
        {
            std::string producer = "producer:0";
            std::string consumer = "consumer:0";
            unsigned    limit    = 10;
        };

    private:
        const Config    m_Config;
        ConnectionFace* m_Connection;
        uint64_t        m_Position = 0;

        void init()
        {
            const std::string sQuery = fmt::format(
                "SELECT position"
                "  FROM message_state"
                " WHERE service='{}'",
                m_Config.consumer);
            m_Connection->Query(sQuery);
            m_Connection->Use([this](const MySQL::Row& aRow) { m_Position = aRow[0].as_uint64(); });
        }

    public:
        struct Task
        {
            uint64_t    serial = 0;
            std::string task;
            std::string hash;

            auto as_tuple() const { return std::tie(serial, task, hash); }
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
                "SELECT serial, task, hash"
                "  FROM message_queue"
                " WHERE producer = '{}' AND serial > {}"
                " ORDER BY serial ASC"
                " LIMIT {}",
                m_Config.producer,
                m_Position,
                m_Config.limit);
            m_Connection->Query(sQuery);
            m_Connection->Use([&sTasks](const MySQL::Row& aRow) {
                Task sTask;
                sTask.serial = aRow[0].as_uint64();
                sTask.task   = aRow[1].as_string();
                sTask.hash   = aRow[2].as_string();
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
                "INSERT INTO message_state(service, position) "
                "VALUES ('{}', {}) "
                "ON DUPLICATE KEY UPDATE position=VALUES(position)",
                m_Config.consumer,
                m_Position);
            m_Connection->Query(sQuery);
        }
    };
} // namespace MySQL::MessageQueue