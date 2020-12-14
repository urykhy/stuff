#pragma once

#include <mysql.h>
#include <string>
#include <list>
#include <stdexcept>

#include <parser/Atoi.hpp>
#include <mpl/Mpl.hpp>

namespace MySQL
{
    struct Config
    {
        std::string host;
        unsigned port = 3306;
        std::string username;
        std::string password;
        std::string database;
        time_t timeout = 10;
        std::string program_name = "";
    };

    class Row
    {
        Row(const Row& ) = delete;
        Row& operator=(const Row& ) = delete;
        const MYSQL_ROW& m_Row;
        const unsigned m_Size;

        class Cell
        {
            const char* m_Ptr = nullptr;

        public:
            Cell(const char* aPtr) : m_Ptr(aPtr) {}

            int64_t as_int64() const
            {
                if (is_null())
                    throw std::logic_error(std::string("MySQL::Null"));
                return Parser::Atoi<int64_t>(m_Ptr);
            }
            std::string as_string() const
            {
                if (is_null())
                    throw std::logic_error(std::string("MySQL::Null"));
                return m_Ptr;
            }
            bool is_null() const { return m_Ptr == nullptr; }
        };

    public:
        Row(const MYSQL_ROW& aRow, const unsigned aSize)
        : m_Row(aRow), m_Size(aSize) {}

        Cell operator[](unsigned index) const {
            if (index >= m_Size)
                throw std::out_of_range(std::string("MySQL::Row"));
            return Cell{m_Row[index]};
        }
    };

    // get mysql enum from type
    template <class T> constexpr enum_field_types GetEnum();
    template <> constexpr enum_field_types GetEnum<char>()          { return MYSQL_TYPE_TINY; }
    template <> constexpr enum_field_types GetEnum<short>()         { return MYSQL_TYPE_SHORT; }
    template <> constexpr enum_field_types GetEnum<int>()           { return MYSQL_TYPE_LONG; }
    template <> constexpr enum_field_types GetEnum<long long int>() { return MYSQL_TYPE_LONGLONG; }
    template <> constexpr enum_field_types GetEnum<float>()         { return MYSQL_TYPE_FLOAT; }
    template <> constexpr enum_field_types GetEnum<double>()        { return MYSQL_TYPE_DOUBLE; }

    class Statment
    {
        Statment(const Statment&) = delete;
        Statment operator=(const Statment&) = delete;

        MYSQL_STMT* m_Stmt = nullptr;
        void cleanup()
        {
            if (m_Stmt != nullptr)
            {
                mysql_stmt_close(m_Stmt);
                m_Stmt = nullptr;
            }
        }

        void bind_one(MYSQL_BIND& aParam, const char* aValue)
        {
            aParam.buffer_type = MYSQL_TYPE_STRING;
            aParam.buffer = const_cast<char*>(aValue);
            aParam.buffer_length = strlen(aValue);
            aParam.is_null=0;
            aParam.length = &aParam.buffer_length;
        }

        template<class T>
        typename std::enable_if<
            std::is_arithmetic<T>::value, void
        >::type
        bind_one(MYSQL_BIND& aParam, const T& aValue)
        {
            aParam.buffer_type = GetEnum<typename std::make_signed<T>::type>();
            aParam.buffer = const_cast<int*>(&aValue);
            aParam.buffer_length = sizeof(aValue);
            aParam.is_null=0;
            aParam.length = &aParam.buffer_length;
            aParam.is_unsigned = std::is_unsigned<T>::value;
        }

        void report(const char* aMsg)
        {
            const std::string sMsg = std::string(aMsg) + ": " + mysql_stmt_error(m_Stmt);
            throw std::runtime_error(sMsg);
        }

        using ResultRow = std::vector<std::string_view>;
        template<class T>
        ResultRow prepareRow(const T* aBind, size_t aCount)
        {
            ResultRow sRow;
            sRow.reserve(aCount);
            for (size_t i = 0; i < aCount; i++)
                sRow.push_back(std::string_view((const char*)aBind[i].buffer, *aBind[i].length));
            return sRow;
        }

    public:
        Statment(MYSQL* aHandle, const std::string& aQuery)
        {
            m_Stmt = mysql_stmt_init(aHandle);
            if (!m_Stmt)
                throw std::runtime_error("mysql_stmt_init");

            if (mysql_stmt_prepare(m_Stmt, aQuery.c_str(), aQuery.size()))
            {
                const std::string sMsg = std::string("mysql_stmt_prepare: ") + mysql_stmt_error(m_Stmt);
                cleanup();
                throw std::runtime_error(sMsg);
            }
        }
        Statment(Statment&& aParent)
        {
            cleanup();
            m_Stmt = aParent.m_Stmt;
            aParent.m_Stmt = nullptr;
        }
        ~Statment()
        {
            cleanup();
        }

        unsigned count() const { return mysql_stmt_param_count(m_Stmt); }

        template<class... T>
        void Execute(const T&... t)
        {
            MYSQL_BIND sBind[sizeof...(t)];
            memset(sBind, 0, sizeof(sBind));

            Mpl::for_each_argument([this, &sBind, index = 0](const auto& x) mutable {
                this->bind_one(sBind[index++], x);
            }, t...);

            if (mysql_stmt_bind_param(m_Stmt, sBind))
                report("mysql_stmt_bind_param");

            if(mysql_stmt_execute(m_Stmt))
                report("mysql_stmt_execute");
        }

        // can crash on large fields, since use alloca to allocate buffers
        template<class T>
        void Use(T aHandler)
        {
            auto sMeta = mysql_stmt_result_metadata(m_Stmt);
            int sFields = mysql_num_fields(sMeta);
            //std::cout << "column count: " << sFields << std::endl;

            MYSQL_BIND sResult[sFields];
            memset(sResult, 0, sizeof(sResult));

            for (int i = 0; i < sFields; i++)
            {
                MYSQL_FIELD* sField = &sMeta->fields[i];
                //std::cout << "field length " << sField->length << ", name = " << sField->name << std::endl;
                sResult[i].buffer = alloca(sField->length);
                sResult[i].buffer_length = sField->length;
                sResult[i].length = (long unsigned int*)alloca(sizeof(unsigned long));
                sResult[i].error  = (my_bool*)alloca(sizeof(my_bool));
            }
            mysql_free_result(sMeta);

            if (mysql_stmt_bind_result(m_Stmt, sResult))
                report("mysql_stmt_bind_result");

            // To cause the complete result set to be buffered on the client
            //if (mysql_stmt_store_result(m_Stmt))
            //    report("mysql_stmt_store_result");

            //std::cout << "row count:    " << mysql_stmt_num_rows(m_Stmt) << std::endl;

            int sCode = 0;
            while (sCode == 0)
            {
                sCode = mysql_stmt_fetch(m_Stmt);
                if (sCode == 1)
                    report("mysql_stmt_fetch");
                if (sCode == 0)
                    aHandler(prepareRow(&sResult[0], sFields));
            };
        }
    };

    struct ConnectionFace
    {
        using UseCB = std::function<void(Row&&)>;

        virtual void open()   = 0;
        virtual void close()  = 0;
        virtual bool ping()   = 0;
        virtual void ensure() = 0;

        virtual void Query(const std::string& aQuery) = 0;
        virtual void Use(UseCB aHandler) = 0;

        virtual ~ConnectionFace() {};
    };

    class Connection : public ConnectionFace
    {
        const Config m_Cfg;
        MYSQL m_Handle;
        bool m_Closed = true;
        Connection(const Connection& ) = delete;
        Connection& operator=(const Connection& ) = delete;

        void report(const char* aMsg)
        {
            throw Error(std::string(aMsg) + ": " + mysql_error(&m_Handle));
        }
    public:

        using Error = std::runtime_error;

        Connection(const Config& aCfg)
        : m_Cfg(aCfg)
        {
            open();
        }

        void open() override
        {
            if (!m_Closed)
                throw Error("connection already open");

            int sReconnectTimeout = 1;
            mysql_init(&m_Handle);
            mysql_options(&m_Handle, MYSQL_OPT_CONNECT_TIMEOUT, &sReconnectTimeout);
            mysql_options(&m_Handle, MYSQL_OPT_READ_TIMEOUT, &m_Cfg.timeout);
            mysql_options(&m_Handle, MYSQL_OPT_WRITE_TIMEOUT, &m_Cfg.timeout);
            if (!m_Cfg.program_name.empty())
                mysql_options4(&m_Handle, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", m_Cfg.program_name.c_str());
            if (!mysql_real_connect(&m_Handle, m_Cfg.host.data(), m_Cfg.username.data(), m_Cfg.password.data(), m_Cfg.database.data(), m_Cfg.port, NULL, 0))
            {
                mysql_close(&m_Handle);
                report("mysql_real_connect");
            }
            m_Closed = false;
        }

        void close() override
        {
            if (!m_Closed)
            {
                mysql_close(&m_Handle);
                m_Closed = true;
            }
        }

        // return true if server alive
        bool ping() override
        {
            return m_Closed ? false : 0 == mysql_ping(&m_Handle);
        }

        void ensure() override
        {
            if (!ping())
            {
                close();
                open();
            }
        }

        virtual ~Connection()
        {
            close();
        }

        Statment Prepare(const std::string& aQuery)
        {
            if (m_Closed)
                throw Error("attempt to use closed connection");

            return Statment(&m_Handle, aQuery);
        }

        void Query(const std::string& aQuery) override
        {
            if (m_Closed)
                throw Error("attempt to use closed connection");

            int rc = mysql_query(&m_Handle, aQuery.data());
            if (rc)
                report("mysql_query");
        }

        void Use(UseCB aHandler) override
        {
            // reads the entire result of a query to the client
            //MYSQL_RES* sResult = mysql_store_result(&m_Handle);

            // initiates a result set retrieval but does not actually read the result set into the client
            MYSQL_RES* sResult = mysql_use_result(&m_Handle);
            if (sResult == NULL)
                report("mysql_use_result");

            int sFields = mysql_num_fields(sResult);
            MYSQL_ROW sRow;
            while ((sRow = mysql_fetch_row(sResult)))
                aHandler(Row(sRow, sFields));
            mysql_free_result(sResult);
        }
    };
}
