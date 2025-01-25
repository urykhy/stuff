#pragma once

#include <fmt/core.h>

#include <boost/asio/awaitable.hpp>
#include <boost/noncopyable.hpp>

#include <libpq-fe.h>
#include <unsorted/Log4cxx.hpp>

namespace PQ {
    inline log4cxx::LoggerPtr sLogger = Logger::Get("sql");

    struct Error : public std::runtime_error
    {
        Error(const std::string& aMsg, PGconn* aConn = nullptr)
        : std::runtime_error(fmt::format("{}{}", aMsg, (aConn == nullptr ? std::string{} : std::string(": ") + PQerrorMessage(aConn))))
        {
        }
    };

    class Client;
    class Row : public boost::noncopyable
    {
        friend class Client;
        const int       m_Row;
        const int       m_MaxColumn;
        const PGresult* m_Res;

        Row(int aRow, PGresult* aRes)
        : m_Row(aRow)
        , m_MaxColumn(PQnfields(aRes))
        , m_Res(aRes)
        {
        }

        void Ensure(int aColumn) const
        {
            if (aColumn >= m_MaxColumn) {
                throw std::out_of_range("PQ::Row index out of range");
            }
        }

    public:
        const char* Name(int aColumn) const
        {
            Ensure(aColumn);
            return PQfname(m_Res, aColumn);
        }
        std::string_view Get(int aColumn) const
        {
            Ensure(aColumn);
            return {PQgetvalue(m_Res, m_Row, aColumn), (std::string_view::size_type)PQgetlength(m_Res, m_Row, aColumn)};
        }
        bool IsNull(int aColumn) const
        {
            Ensure(aColumn);
            return PQgetisnull(m_Res, m_Row, aColumn);
        }
        int Size() const
        {
            return m_MaxColumn;
        }
    };

    struct Params
    {
        bool single_row_mode = false;
    };

    class Client : public boost::noncopyable
    {
        const Params m_Params;
        PGconn*      m_Conn = nullptr;

        boost::asio::awaitable<void> Wait(auto aType)
        {
            boost::asio::ip::tcp::socket sSocket(co_await boost::asio::this_coro::executor, boost::asio::ip::tcp::v4(), PQsocket(m_Conn));
            co_await sSocket.async_wait(aType, boost::asio::use_awaitable);
            sSocket.release();
            co_return;
        }

        boost::asio::awaitable<void> Use(std::function<void(Row&&)>& aHandler)
        {
            bool sDone = false;
            DEBUG("read response");
            while (!sDone) {
                co_await Wait(boost::asio::ip::tcp::socket::wait_read);
                if (!PQconsumeInput(m_Conn)) {
                    throw Error("PQconsumeInput failed", m_Conn);
                }
                while (!PQisBusy(m_Conn)) {
                    std::unique_ptr<PGresult, decltype(&PQclear)> sRes(PQgetResult(m_Conn), PQclear);
                    if (sRes.get() == nullptr) {
                        sDone = true;
                        break; // end of response
                    }
                    const auto sStatus = PQresultStatus(sRes.get());
                    if (sStatus == PGRES_COMMAND_OK) {
                        DEBUG("... query without response");
                        sDone = true;
                        break;
                    }
                    if (sStatus != PGRES_TUPLES_OK and sStatus != PGRES_SINGLE_TUPLE) {
                        throw Error("unexpected PQresultStatus (" + std::to_string(sStatus) + ")");
                    }
                    DEBUG("... got " << PQntuples(sRes.get()) << " rows");
                    if (aHandler) {
                        for (int sRow = 0; sRow < PQntuples(sRes.get()); sRow++) {
                            aHandler(Row(sRow, sRes.get()));
                        }
                    }
                }
            }
            DEBUG("done");
            co_return;
        }

    public:
        Client(const Params& aParams)
        : m_Params(aParams)
        {
        }

        ~Client()
        {
            if (m_Conn) {
                PQfinish(m_Conn);
            }
        }

        boost::asio::awaitable<void> Connect(const std::string& aRemote)
        {
            INFO("connecting to " << aRemote);
            m_Conn = PQconnectStart(aRemote.c_str());
            if (!m_Conn) {
                throw Error("PQconnectStart failed: OOM");
            }
            auto sStatus = PQstatus(m_Conn);
            if (sStatus != CONNECTION_STARTED) {
                throw Error("PQconnectStart failed", m_Conn);
            }

            if (!PQisnonblocking(m_Conn) && PQsetnonblocking(m_Conn, 1) == -1) {
                throw Error("PQsetnonblocking failed", m_Conn);
            }

            while (true) {
                auto sPoll = PQconnectPoll(m_Conn);
                sStatus    = PQstatus(m_Conn);
                if (sStatus == CONNECTION_BAD or sStatus == CONNECTION_OK) {
                    break;
                }
                co_await Wait(sPoll == PGRES_POLLING_WRITING ? boost::asio::ip::tcp::socket::wait_write : boost::asio::ip::tcp::socket::wait_read);
            }
            if (sStatus != CONNECTION_OK) {
                throw Error("Connection failed", m_Conn);
            }

            co_return;
        }

        boost::asio::awaitable<bool> Ping()
        {
            co_return PQstatus(m_Conn) == CONNECTION_OK;
        }

        boost::asio::awaitable<void> Query(const std::string& aQuery, std::function<void(Row&&)>&& aHandler = {})
        {
            if (!co_await Ping()) {
                throw Error("Connection not ready", m_Conn);
            }

            INFO("query " << aQuery);
            if (!PQsendQuery(m_Conn, aQuery.c_str())) {
                throw Error("PQsendQuery failed", m_Conn);
            }

            if (m_Params.single_row_mode) {
                if (!PQsetSingleRowMode(m_Conn)) {
                    throw Error("PQsetSingleRowMode failed", m_Conn);
                }
            }

            while (true) {
                if (auto sCode = PQflush(m_Conn); sCode == -1) {
                    throw Error("PQflush failed", m_Conn);
                } else if (sCode == 0) {
                    break;
                }
                // FIXME: must wait_read as well and call PQconsumeInput
                co_await Wait(boost::asio::ip::tcp::socket::wait_write);
            }

            co_return co_await Use(aHandler);
        }
    };
} // namespace PQ
