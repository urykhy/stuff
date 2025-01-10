#pragma once

#define FDB_API_VERSION 730
#include <fmt/core.h>
#include <foundationdb/fdb_c.h>

#include <iostream>
#include <optional>
#include <string_view>
#include <thread>

#include <boost/asio/awaitable.hpp>
#include <boost/core/noncopyable.hpp>

#include <threads/Coro.hpp>

namespace FDB {

    class Error : public std::runtime_error
    {
        const int m_Error;

    public:
        Error(const std::string& aMsg, fdb_error_t aError)
        : std::runtime_error(fmt::format("{}: {} ({})", aMsg, fdb_get_error(aError), aError))
        , m_Error(aError)
        {
        }

        int Code() const { return m_Error; }
    };

    namespace {
        static void Check(fdb_error_t aError, const std::string& aMsg)
        {
            if (aError) {
                throw Error(aMsg, aError);
            }
        }

        class Guard
        {
            std::thread m_Thread;

        public:
            Guard()
            {
                Check(fdb_select_api_version(FDB_API_VERSION), "select api version");
                Check(fdb_setup_network(), "setup network");

                m_Thread = std::thread([]() {
                    if (auto sError = fdb_run_network(); sError) {
                        std::cerr << "FDB::run network error: " << fdb_get_error(sError) << std::endl;
                    }
                });
            }

            ~Guard()
            {
                if (auto sError = fdb_stop_network(); sError) {
                    std::cerr << "FDB::stop network error: " << fdb_get_error(sError) << std::endl;
                }
                if (m_Thread.joinable()) {
                    m_Thread.join();
                }
            }
        };
    } // namespace

    static inline Guard sGuard;

    class Client;
    class Transaction;
    class Future;

    class Client : public boost::noncopyable
    {
        friend class Transaction;
        FDBDatabase* m_DB{nullptr};

    public:
        Client()
        {
            Check(fdb_create_database(nullptr, &m_DB), "create database");
        }

        ~Client()
        {
            fdb_database_destroy(m_DB);
        }
    };

    class Future : public boost::noncopyable
    {
        friend class Transaction;
        FDBFuture* m_Future;

        static void callback(FDBFuture*, void* aWaiter)
        {
            ((Threads::Coro::Waiter*)aWaiter)->notify();
        }

        Future(FDBFuture* aFuture)
        : m_Future(aFuture)
        {
        }

    public:
        ~Future()
        {
            fdb_future_destroy(m_Future);
        }

        void Wait()
        {
            Check(fdb_future_block_until_ready(m_Future), "future block until ready");
            Check(fdb_future_get_error(m_Future), "future get error");
        }

        boost::asio::awaitable<void> CoWait()
        {
            Threads::Coro::Waiter sWaiter;
            Check(fdb_future_set_callback(m_Future, &Future::callback, &sWaiter), "future set callback");
            co_await sWaiter.wait(co_await boost::asio::this_coro::executor);
            Check(fdb_future_get_error(m_Future), "future get error");
        }

        std::optional<std::string_view> Get()
        {
            fdb_bool_t     sExists = false;
            const uint8_t* sPtr    = nullptr;
            int            sSize   = 0;
            Check(fdb_future_get_value(m_Future, &sExists, &sPtr, &sSize), "future get value");
            if (!sExists) {
                return std::nullopt;
            }
            return std::string_view((const char*)sPtr, sSize);
        }
    };

    class Transaction : public boost::noncopyable
    {
        FDBTransaction* m_Transaction{nullptr};

    public:
        Transaction(Client& aClient, uint64_t aTimeoutMs = 100)
        {
            Check(fdb_database_create_transaction(aClient.m_DB, &m_Transaction), "create transaction");
            if (aTimeoutMs > 0) {
                Check(fdb_transaction_set_option(m_Transaction, FDB_TR_OPTION_TIMEOUT, (uint8_t*)&aTimeoutMs, sizeof(aTimeoutMs)), "set transaction timeout");
            }
        }

        ~Transaction()
        {
            fdb_transaction_destroy(m_Transaction);
        }

        Future Get(std::string_view aKey)
        {
            return fdb_transaction_get(m_Transaction, (uint8_t*)aKey.data(), aKey.size(), true /* snapshot*/);
        }

        void Set(std::string_view aKey, std::string_view aValue)
        {
            fdb_transaction_set(m_Transaction, (uint8_t*)aKey.data(), aKey.size(), (uint8_t*)aValue.data(), aValue.size());
        }

        void Erase(std::string_view aKey)
        {
            fdb_transaction_clear(m_Transaction, (uint8_t*)aKey.data(), aKey.size());
        }

        void Commit()
        {
            Future sFuture(fdb_transaction_commit(m_Transaction));
            sFuture.Wait();
        }

        boost::asio::awaitable<void> CoCommit()
        {
            Future sFuture(fdb_transaction_commit(m_Transaction));
            co_await sFuture.CoWait();
        }
    };

} // namespace FDB
