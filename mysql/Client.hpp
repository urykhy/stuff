#pragma once

#include <mysql.h>
#include <string>
#include <list>
#include <stdexcept>

namespace MySQL
{
    struct Config
    {
        std::string host;
        unsigned port = 3306;
        std::string username;
        std::string password;
        std::string database;
    };

    class Row
    {
        Row(const Row& ) = delete;
        Row& operator=(const Row& ) = delete;
        const MYSQL_ROW& m_Row;
        const unsigned m_Size;

        void validate(unsigned id) const
        {
            if (id >= m_Size)
                throw std::out_of_range(std::string("MySQL::Row"));
            if (m_Row[id] == nullptr)
                throw std::logic_error(std::string("MySQL::Null"));
        }

    public:
        Row(const MYSQL_ROW& aRow, const unsigned aSize)
        : m_Row(aRow), m_Size(aSize) {}

        int as_int(unsigned id) const
        {
            validate(id);
            return std::atoi(m_Row[id]);
        }

        std::string as_str(unsigned id) const
        {
            validate(id);
            return m_Row[id];
        }
    };

    class Connection
    {
        MYSQL m_Handle;
        Connection(const Connection& ) = delete;
        Connection& operator=(const Connection& ) = delete;
    public:
        Connection(const Config& aCfg)
        {
            mysql_init(&m_Handle);
            if (!mysql_real_connect(&m_Handle, aCfg.host.data(), aCfg.username.data(), aCfg.password.data(), aCfg.database.data(), aCfg.port, NULL, 0))
                throw std::runtime_error("mysql_real_connect");
        }

        ~Connection()
        {
            mysql_close(&m_Handle);
        }


        void Query(const std::string& aQuery)
        {
            int rc = mysql_query(&m_Handle, aQuery.data());
            if (rc)
                throw std::runtime_error("mysql_query");
        }

        template<class T>
        void Use(T aHandler)
        {
            // reads the entire result of a query to the client
            //MYSQL_RES* sResult = mysql_store_result(&m_Handle);

            // initiates a result set retrieval but does not actually read the result set into the client
            MYSQL_RES* sResult = mysql_use_result(&m_Handle);
            if (sResult == NULL)
                throw std::runtime_error("mysql_store_result");

            int sFields = mysql_num_fields(sResult);
            MYSQL_ROW sRow;
            while ((sRow = mysql_fetch_row(sResult)))
                aHandler(Row(sRow, sFields));
            mysql_free_result(sResult);
        }

        bool ping()
        {
            return 0 == mysql_ping(&m_Handle);
        }
    };

};
