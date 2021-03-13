#pragma once

#include <sqlite3.h>

#include <functional>
#include <stdexcept>
#include <string>

#include <boost/core/noncopyable.hpp>

#include <exception/Error.hpp>
#include <mpl/Mpl.hpp>
#include <unsorted/Log4cxx.hpp>
#include <unsorted/Raii.hpp>

// https://www.sqlite.org/lang.html
// https://www.sqlite.org/draft/cintro.html
// https://sqlite.org/c3ref/funclist.html

namespace Lite {

    class DB;

    using Error = Exception::Error<DB>;

    inline void Check(int aRC)
    {
        if (aRC != SQLITE_OK)
            throw Error("Error " + std::to_string(aRC) + ": " + sqlite3_errstr(aRC));
    }

    class Statment : public boost::noncopyable
    {
        sqlite3*                 m_Handle   = nullptr;
        sqlite3_stmt*            m_Statment = nullptr;
        unsigned                 m_Columns  = 0;
        std::vector<const char*> m_Names;

        friend class DB;
        Statment(sqlite3* aHandle, sqlite3_stmt* aStatment)
        : m_Handle(aHandle)
        , m_Statment(aStatment)
        , m_Columns(sqlite3_column_count(m_Statment))
        {
            m_Names.reserve(m_Columns);
            for (unsigned i = 0; i < m_Columns; i++)
                m_Names.push_back(sqlite3_column_name(m_Statment, i));
        }

    public:
        template <class... T>
        void Assign(T&&... a)
        {
            unsigned sIndex = 1;
            Mpl::for_each_argument(
                [this, &sIndex](auto&& aValue) {
                    using X = std::decay_t<decltype(aValue)>;
                    if constexpr (std::numeric_limits<X>::is_integer) {
                        Check(sqlite3_bind_int64(m_Statment, sIndex, aValue));
                    } else if constexpr (std::is_same_v<X, std::string_view>) {
                        Check(sqlite3_bind_text(m_Statment, sIndex, aValue.data(), aValue.size(), SQLITE_STATIC));
                    } else {
                        throw std::invalid_argument("assign: not supported type");
                    }
                    sIndex++;
                },
                a...);
        }

        template <class T>
        void Use(T&& aHandler)
        {
            Util::Raii  sCleanup([this]() { Reset(); });
            const char* sValues[m_Columns] = {};

            do {
                int sCode = sqlite3_step(m_Statment);
                if (sCode == SQLITE_DONE)
                    return;
                if (sCode == SQLITE_ROW) {
                    for (unsigned i = 0; i < m_Columns; i++) {
                        sValues[i] = (const char*)sqlite3_column_text(m_Statment, i);
                        auto sCode = sqlite3_errcode(m_Handle);
                        if (SQLITE_ROW != sCode)
                            Check(sCode); // ensure no OOM occured
                    }
                    aHandler(m_Columns, (char const* const*)&sValues, (char const* const*)&m_Names[0]);
                } else {
                    Check(sCode);
                }
            } while (true);
        }

        void Reset()
        {
            sqlite3_reset(m_Statment);
            sqlite3_clear_bindings(m_Statment);
        }

        ~Statment()
        {
            sqlite3_finalize(m_Statment);
        }
    };

    class DB : public boost::noncopyable
    {
        sqlite3* m_Handle = nullptr;

        static int Callback(void* aCB, int aColumns, char** aValues, char** aNames)
        {
            if (aCB == nullptr)
                return -1;
            try {
                static_cast<CB*>(aCB)->operator()(aColumns, aValues, aNames);
                return 0;
            } catch (const std::exception& e) {
                WARN("callback error: " << e.what());
                return -1;
            }
        }

    public:
        using CB = std::function<void(int aColumns, char const* const* aValues, char const* const* aNames)>;

        DB(const std::string& aPath = ":memory:", int aFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
        {
            Check(sqlite3_open_v2(aPath.c_str(), &m_Handle, aFlags, nullptr));
        }

        void Query(const std::string& aQuery, CB&& aCB)
        {
            log4cxx::NDC ndc("lite");
            DEBUG("query: " << aQuery);
            Check(sqlite3_exec(m_Handle, aQuery.c_str(), Callback, &aCB, nullptr));
        }

        void Query(const std::string& aQuery)
        {
            log4cxx::NDC ndc("lite");
            DEBUG("query: " << aQuery);
            Check(sqlite3_exec(m_Handle, aQuery.c_str(), Callback, nullptr, nullptr));
        }

        void close()
        {
            Check(sqlite3_close(m_Handle));
            m_Handle = nullptr;
        }

        int64_t LastInsertRowid()
        {
            return sqlite3_last_insert_rowid(m_Handle);
        }

        void Backup(DB& aDst, const std::string& aDB = "main")
        {
            auto sHandle = sqlite3_backup_init(aDst.m_Handle, aDB.c_str(), m_Handle, aDB.c_str());
            if (sHandle == nullptr)
                Check(sqlite3_errcode(aDst.m_Handle));
            Util::Raii sCleanup([sHandle]() { sqlite3_backup_finish(sHandle); });
            int        sRC = sqlite3_backup_step(sHandle, -1);
            if (sRC != SQLITE_DONE)
                Check(sRC);
        }

        Statment Prepare(const std::string& aQuery)
        {
            sqlite3_stmt* sData = nullptr;
            Check(sqlite3_prepare(m_Handle, aQuery.c_str(), aQuery.size() + 1, &sData, nullptr));
            return Statment(m_Handle, sData);
        }

        ~DB()
        {
            try {
                close();
            } catch (...) {
            }
        }
    };

} // namespace Lite