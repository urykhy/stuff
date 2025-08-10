#pragma once

#include <fmt/core.h>

#include <chrono>
#include <optional>

#include "Client.hpp"
#include "Quote.hpp"

#include <threads/Group.hpp>

namespace MySQL::Workflow {
    struct Config
    {
        std::string table = "workflow";
    };

    struct Task
    {
        uint64_t                   id = 0;
        std::string                task;
        std::string                space;
        std::optional<std::string> cookie;

        Task(const MySQL::Row& aRow)
        : id(aRow[0])
        , task(aRow[1])
        , space(aRow[2])
        {
            if (aRow[3])
                cookie = aRow[3];
        }
    };

    struct Status
    {
        std::string                task;
        std::string                status;
        std::optional<std::string> cookie;

        Status(const MySQL::Row& aRow)
        {
            task   = aRow[0];
            status = aRow[1];
            if (aRow[2])
                cookie = aRow[2];
        }
    };

    // not thread safe
    class Manager
    {
        Config          m_Config;
        ConnectionFace* m_Connection;

    public:
        Manager(const Config& aConfig, ConnectionFace* aConnection)
        : m_Config(aConfig)
        , m_Connection(aConnection)
        {
        }

        std::optional<Task> get(const std::string& aSpace, const std::string& aTaskPrefix, const std::string& aWorker)
        {
            std::optional<Task> sTask;
            try {
                m_Connection->ensure();
                m_Connection->Query("BEGIN"); // start transaction for `SELECT FOR UPDATE`

                std::string_view sSpacePrefix;
                {
                    auto sPos = aSpace.find('/');
                    if (sPos != std::string::npos) {
                        sSpacePrefix = std::string_view(aSpace.c_str(), sPos);
                    } else {
                        sSpacePrefix = std::string_view(aSpace);
                    }
                }
                const std::string sWhere = "(status = 'new' OR (status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)))";

                m_Connection->Query(fmt::format(
                    "  WITH s AS (SELECT DISTINCT strand FROM {0} WHERE status IN ('started') AND space LIKE '{2}/%' AND strand IS NOT NULL) "
                    "SELECT id, task, space, cookie"
                    "  FROM {0} w"
                    " WHERE {4}"
                    "   AND w.strand NOT IN (SELECT * FROM s)"
                    "   AND space LIKE '{1}%'"
                    "   AND task LIKE '{3}/%'"
                    " ORDER BY priority DESC, id ASC"
                    " LIMIT 1 FOR UPDATE SKIP LOCKED",
                    m_Config.table, aSpace, sSpacePrefix, aTaskPrefix, sWhere));
                m_Connection->Use([&sTask](const MySQL::Row& aRow) {
                    sTask.emplace(aRow);
                });

                if (!sTask) {
                    m_Connection->Query("COMMIT");
                    return {};
                }

                // use sWhere condition, to fight duplicates from FOR UPDATE
                m_Connection->Query(fmt::format(
                    "UPDATE {0} SET status = 'started', worker = '{2}' WHERE id = {1} AND {3}",
                    m_Config.table, sTask->id, aWorker, sWhere));

                // ensure we pick task
                m_Connection->Query("SELECT ROW_COUNT()");
                unsigned sRowCount = 0;
                m_Connection->Use([&sRowCount](const MySQL::Row& aRow) {
                    sRowCount = aRow[0];
                });
                if (sRowCount != 1) {
                    WARN("Workflow: unexpected ROW_COUNT after UPDATE, must be 1, but actual is " << sRowCount << ", rollback");
                    m_Connection->Query("ROLLBACK");
                    return {};
                }

                m_Connection->Query("COMMIT");
                return sTask;
            } catch (const std::exception& sEx) {
                WARN("Exception: " << sEx.what());
                return {};
            }
        }

        // TODO: start new tasks in transaction
        void spawn()
        {
        }

        // aCookie must be quoted
        // TODO: return flow status ?
        void update(uint64_t aID, const std::string& aCookie)
        {
            const std::string sQuery = fmt::format(
                "UPDATE {0} SET cookie = '{2}' WHERE id = {1} AND status IN ('started')",
                m_Config.table,
                aID,
                aCookie);
            m_Connection->ensure();
            m_Connection->Query(sQuery);
        }

        void done(uint64_t aID, bool aSuccess)
        {
            const std::string sQuery = fmt::format(
                "UPDATE {0} SET status = '{2}' WHERE id = {1} AND status IN ('started','done')",
                m_Config.table,
                aID,
                aSuccess ? "done" : "error");
            m_Connection->ensure();
            m_Connection->Query(sQuery);
        }

        std::vector<Status> status(const std::string& aSpace, const std::string aTaskPrefix)
        {
            std::vector<Status> sResult;
            m_Connection->Query(fmt::format(
                "SELECT task, status, cookie"
                "  FROM {0}"
                " WHERE space LIKE '{1}%'"
                "   AND task LIKE '{2}/%'",
                m_Config.table, aSpace, aTaskPrefix));
            m_Connection->Use([&sResult](const MySQL::Row& aRow) {
                sResult.push_back(Status(aRow));
            });
            return sResult;
        }
    };
} // namespace MySQL::Workflow
