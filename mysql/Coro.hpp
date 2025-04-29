#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "Client.hpp"

namespace MySQL::Coro {

    class Connection
    {
        const Config m_Cfg;
        MYSQL        m_Handle;
        bool         m_Closed = true;

        void Report(const char* aMsg)
        {
            throw Error(std::string(aMsg) + ": " + mysql_error(&m_Handle), mysql_errno(&m_Handle));
        }

        [[nodiscard]] boost::asio::awaitable<void> Wait()
        {
            boost::asio::ip::tcp::socket sSocket(co_await boost::asio::this_coro::executor, boost::asio::ip::tcp::v4(), m_Handle.net.fd);
            co_await sSocket.async_wait(boost::asio::ip::tcp::socket::wait_read, boost::asio::use_awaitable);
            sSocket.release();
            co_return;
        }

        template <class T>
        [[nodiscard]] boost::asio::awaitable<void> Do(T&& aHandler, const char* aName)
        {
            net_async_status sStatus = {};
            while ((sStatus = aHandler()) == NET_ASYNC_NOT_READY) {
                co_await Wait();
            };
            if (sStatus != NET_ASYNC_COMPLETE) {
                Report(aName);
            }
            co_return;
        }

    public:
        Connection(const Config& aCfg)
        : m_Cfg(aCfg)
        {
        }

        ~Connection()
        {
            Close();
        }

        [[nodiscard]] boost::asio::awaitable<void> Open()
        {
            if (!m_Closed)
                throw Error("connection already open");
            m_Closed = false;

            INFO("connecting to " << m_Cfg.username << "@" << m_Cfg.host << ":" << m_Cfg.port << "/" << m_Cfg.database);
            int sReconnectTimeout = 1;
            mysql_init(&m_Handle);
            mysql_options(&m_Handle, MYSQL_OPT_CONNECT_TIMEOUT, &sReconnectTimeout);
            mysql_options(&m_Handle, MYSQL_OPT_READ_TIMEOUT, &m_Cfg.timeout);
            mysql_options(&m_Handle, MYSQL_OPT_WRITE_TIMEOUT, &m_Cfg.timeout);
            if (!m_Cfg.program_name.empty())
                mysql_options4(&m_Handle, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", m_Cfg.program_name.c_str());

            co_await Do([&, this]() { return mysql_real_connect_nonblocking(&m_Handle, m_Cfg.host.data(), m_Cfg.username.data(), m_Cfg.password.data(), m_Cfg.database.data(), m_Cfg.port, NULL, 0); },
                        "mysql_real_connect_nonlocking");
            co_return;
        }

        void Close()
        {
            if (!m_Closed) {
                mysql_close(&m_Handle);
                m_Closed = true;
            }
        }

        [[nodiscard]] boost::asio::awaitable<void> Query(const std::string& aQuery)
        {
            if (m_Closed)
                throw Error("attempt to use closed connection");

            INFO("query " << aQuery);
            co_await Do([&, this]() { return mysql_real_query_nonblocking(&m_Handle, aQuery.data(), aQuery.size()); },
                        "mysql_real_query_nonblocking");
        }

        using UseCB = std::function<void(Row&&)>;
        [[nodiscard]] boost::asio::awaitable<void> Use(UseCB aHandler)
        {
            std::exception_ptr sException;
            MYSQL_RES*         sResult = nullptr;

            try {
                if (m_Cfg.store_result) {
                    // reads the entire result of a query to the client
                    co_await Do([&, this]() { return mysql_store_result_nonblocking(&m_Handle, &sResult); },
                                "mysql_store_result_nonblocking");
                } else {
                    // initiates a result set retrieval but does not actually read the result set into the client
                    sResult = mysql_use_result(&m_Handle);
                    if (sResult == NULL)
                        Report("mysql_use_result");
                }

                const unsigned                sFields = mysql_num_fields(sResult);
                std::vector<std::string_view> sMeta; // column names
                sMeta.reserve(sFields);
                while (auto sField = mysql_fetch_field(sResult)) {
                    sMeta.push_back(sField->name);
                }

                MYSQL_ROW sRow;
                while (true) {
                    co_await Do([&]() { return mysql_fetch_row_nonblocking(sResult, &sRow); },
                                "mysql_fetch_row_nonblocking");
                    if (sRow == nullptr) {
                        if (mysql_errno(&m_Handle)) {
                            Report("mysql_fetch_row_nonblocking");
                        }
                        break;
                    }
                    aHandler(Row(sRow, sFields, sMeta));
                }
            } catch (...) {
                sException = std::current_exception();
            }

            if (sResult != nullptr) {
                co_await Do([&]() { return mysql_free_result_nonblocking(sResult); },
                            "mysql_free_result_nonblocking");
            }

            if (sException) {
                std::rethrow_exception(sException);
            }

            co_return;
        }
    };

} // namespace MySQL::Coro
