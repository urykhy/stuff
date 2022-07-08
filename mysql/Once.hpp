#pragma once

#include <fmt/core.h>

#include "Client.hpp"

namespace MySQL::Once {

    // string params must be quoted by user
    template <class T>
    void transaction(ConnectionFace* aClient, const std::string& aService, const std::string& aTask, const std::string& aId, T&& aHandler)
    {
        const std::string_view sTable = "transaction_log";
        aClient->Query("BEGIN");

        bool sAlready = false;
        aClient->Query(fmt::format("SELECT COUNT(1) FROM {} WHERE service='{}' AND task='{}' AND id='{}'", sTable, aService, aTask, aId));
        aClient->Use([&sAlready](const MySQL::Row& aRow) { sAlready = aRow[0].as_int64() > 0; });
        if (!sAlready) {
            aClient->Query(fmt::format("INSERT INTO {} (service,task,id) VALUES ('{}','{}','{}')", sTable, aService, aTask, aId));
            aHandler(aClient);
        }
        aClient->Query("COMMIT");
    };

    inline void truncate(ConnectionFace* aClient)
    {
        aClient->Query("CALL truncate_transaction_log()");
    }
} // namespace MySQL::Once