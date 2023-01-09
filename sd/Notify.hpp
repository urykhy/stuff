#pragma once

#include <mutex>

#include <etcd/Etcd.hpp>

namespace SD {
    struct Notify : public std::enable_shared_from_this<Notify>
    {
        struct Params
        {
            Etcd::Client::Params addr;
            std::string          key;
            int                  ttl      = 30;
            int                  period   = 10;
            bool                 no_clear = false;
        };

    private:
        const Params             m_Params;
        boost::asio::io_service& m_Service;

        std::atomic<bool>         m_Stop{false};
        boost::asio::steady_timer m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        int64_t            m_Lease = 0;
        std::string        m_Value;
        bool               m_Refresh = false;
        bool               m_Update  = false;
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

        void init(boost::asio::yield_context yield)
        {
            Etcd::Client sClient(m_Service, m_Params.addr, yield);

            int64_t sLease = getLease();
            if (sLease == 0)
                sLease = sClient.createLease(m_Params.ttl);

            const auto sValue = getValue();
            sClient.put(m_Params.key, sValue, sLease);

            Lock lk(m_Mutex);
            m_Lease   = sLease;
            m_LastError.clear();
            if (sValue != m_Value) // user make new update ?
                return;
            m_Refresh = true;
            m_Update  = false;
        };

        void cleanup(boost::asio::yield_context yield)
        {
            try {
                if (!m_Params.no_clear) {
                    Etcd::Client sClient(m_Service, m_Params.addr, yield);
                    sClient.dropLease(getLease());
                }
            } catch (...) {
            }
        };

        void refresh(boost::asio::yield_context yield)
        {
            try {
                if (m_Refresh) {
                    Etcd::Client sClient(m_Service, m_Params.addr, yield);
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
        Notify(boost::asio::io_service& aService, const Params& aParams, const std::string& aValue)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
        , m_Value(aValue)
        {
        }

        void update(const std::string& aValue)
        {
            Lock lk(m_Mutex);
            m_Value   = aValue;
            m_Refresh = false;
            m_Update  = true;
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
            boost::asio::spawn(m_Timer.get_executor(), [this, p = shared_from_this()](boost::asio::yield_context yield) {
                boost::beast::error_code ec;
                while (!m_Stop) {
                    refresh(yield);
                    if (m_Update)   // user's update pending
                        m_Timer.expires_from_now(std::chrono::milliseconds(10));
                    else
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
} // namespace SD
