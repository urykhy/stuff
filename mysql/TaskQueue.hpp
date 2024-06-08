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
        std::string resume = "status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)";
        std::string extra = {};

        bool isolation = false; // true to resume own tasks only
        bool reverse   = false; // true to pick latest task
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

    // not thread safe
    class Manager
    {
        Config          m_Config;
        ConnectionFace* m_Connection;

        std::string where(const std::string& aWorker) const
        {
            std::string sCond = "status = 'new'";

            if (!m_Config.resume.empty()) {
                std::string sResume = m_Config.resume;
                if (m_Config.isolation)
                    sResume.append(" AND worker = '" + aWorker + "'");
                sCond += " OR (" + sResume + ")";
            }

            if (m_Config.extra.empty())
                return sCond;

            return m_Config.extra + " AND (" + sCond + ")";
        }

    public:
        Manager(const Config& aConfig, ConnectionFace* aConnection)
        : m_Config(aConfig)
        , m_Connection(aConnection)
        {
        }

        std::optional<Task> get(const std::string& aWorker)
        {
            std::optional<Task> sTask;
            const std::string   sWhere = where(aWorker);

            m_Connection->ensure();
            m_Connection->Query("BEGIN"); // start transaction for `SELECT FOR UPDATE`

            // need mysql8 to use skip locked
            m_Connection->Query(fmt::format(
                "SELECT id, task, worker, cookie "
                "FROM {0} "
                "WHERE {1} "
                "ORDER BY id {2} LIMIT 1 FOR UPDATE SKIP LOCKED",
                m_Config.table,
                sWhere,
                m_Config.reverse ? "DESC" : "ASC"));
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
                aWorker));
            m_Connection->Query("COMMIT");

            return sTask;
        }

        // aCookie must be quoted
        void update(uint64_t aID, const std::string& aCookie)
        {
            const std::string sQuery = fmt::format(
                "UPDATE {0} "
                "SET cookie = '{2}' "
                "WHERE id = {1} AND status IN ('started')",
                m_Config.table,
                aID,
                aCookie);
            m_Connection->ensure();
            m_Connection->Query(sQuery);
        }

        void done(uint64_t aID, bool aSuccess)
        {
            const std::string sQuery = fmt::format(
                "UPDATE {0} "
                "SET status = '{2}' "
                "WHERE id = {1} AND status IN ('started','done')",
                m_Config.table,
                aID,
                aSuccess ? "done" : "error");
            m_Connection->ensure();
            m_Connection->Query(sQuery);
        }

        size_t size()
        {
            m_Connection->ensure();
            m_Connection->Query(fmt::format(
                "SELECT count(1) "
                "FROM {0} "
                "WHERE status in ('new','started')",
                m_Config.table));
            size_t sSize = 0;
            m_Connection->Use([&sSize](const MySQL::Row& aRow) { sSize = aRow[0]; });
            return sSize;
        }
    };
} // namespace MySQL::TaskQueue
