#pragma once

#include <mutex>

#include "Etcd.hpp"

namespace Etcd {
    struct Notify : public std::enable_shared_from_this<Notify>
    {
        struct Params
        {
            Client::Params addr;
            std::string    key;
            int            ttl      = 60;
            int            period   = 20;
            bool           no_clear = false;
        };

    private:
        const Params      m_Params;
        asio::io_service& m_Service;
        std::string       m_EtcdValue;

        std::atomic<bool>  m_Stop{false};
        asio::steady_timer m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        int64_t            m_Lease = 0;
        std::string        m_Value;
        bool               m_Refresh = false;
        std::string        m_LastError;

        int64_t getLease()
        {
            Lock lk(m_Mutex);
            return m_Lease;
        }

        std::string getValue()
        {
            Lock lk(m_Mutex);
            return m_Value;
        }

        void init(asio::yield_context yield)
        {
            const auto sValue = getValue();
            Client     sClient(m_Service, m_Params.addr, yield);

            int64_t sLease = getLease();
            if (sLease == 0)
                sLease = sClient.createLease(m_Params.ttl);
            sClient.put(m_Params.key, sValue, sLease);

            Lock lk(m_Mutex);
            m_EtcdValue = sValue;
            m_Lease     = sLease;
            m_Refresh   = true;
            m_LastError.clear();
        };

        void cleanup(asio::yield_context yield)
        {
            try {
                if (!m_Params.no_clear) {
                    Client sClient(m_Service, m_Params.addr, yield);
                    sClient.dropLease(getLease());
                }
            } catch (...) {
            }
        };

        void refresh(asio::yield_context yield)
        {
            try {
                if (m_Refresh) {
                    Client sClient(m_Service, m_Params.addr, yield);
                    sClient.updateLease(getLease());
                } else {
                    init(yield);
                }
            } catch (const std::exception& e) {
                Lock lk(m_Mutex);
                m_Refresh   = false;
                m_LastError = e.what();
            }
        };

    public:
        Notify(asio::io_service& aService, const Params& aParams, const std::string& aValue)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
        , m_Value(aValue)
        {}

        void update(const std::string& aValue)
        {
            Lock lk(m_Mutex);
            m_Value   = aValue;
            m_Refresh = false;
            m_Timer.cancel();
        }

        using Status = std::pair<bool, std::string>;
        Status status() const
        {
            Lock lk(m_Mutex);
            return {m_Refresh, m_LastError};
        }

        void start()
        {
            asio::spawn(m_Timer.get_executor(), [this, p = shared_from_this()](asio::yield_context yield) {
                beast::error_code ec;
                while (!m_Stop) {
                    refresh(yield);
                    m_Timer.expires_from_now(std::chrono::seconds(m_Params.period));
                    m_Timer.async_wait(yield[ec]);
                }
                cleanup(yield);
            });
        }

        void stop()
        {
            m_Stop = true;
            m_Timer.cancel();
        }
    };
} // namespace Etcd