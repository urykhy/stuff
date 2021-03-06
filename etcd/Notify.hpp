#pragma once

#include <mutex>

#include <threads/Periodic.hpp>

#include "Etcd.hpp"

namespace Etcd {
    struct Notify
    {
        struct Params
        {
            Client::Params addr;
            std::string    key;
            int            ttl    = 60;
            int            period = 20;
        };

    private:
        const Params      m_Params;
        Etcd::Client      m_Client;
        Threads::Periodic m_Periodic;
        std::string       m_PrevValue;

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

        void init()
        {
            const auto sValue = getValue();
            int64_t    sLease = m_Client.createLease(m_Params.ttl);
            m_Client.put(m_Params.key, sValue, sLease);

            Lock lk(m_Mutex);
            m_PrevValue = sValue;
            m_Lease     = sLease;
            m_Refresh   = true;
            m_LastError.clear();
        };

        bool updatedValue() const
        {
            Lock lk(m_Mutex);
            return m_PrevValue != m_Value;
        }

        void cleanup()
        {
            try {
                m_Client.dropLease(getLease());
            } catch (...) {
            }
        };

        void refresh()
        {
            try {
                if (m_Refresh and not updatedValue()) {
                    m_Client.updateLease(getLease());
                } else {
                    init();
                }
            } catch (const std::exception& e) {
                Lock lk(m_Mutex);
                m_Refresh   = false;
                m_LastError = e.what();
            }
        };

    public:
        Notify(const Params& aParams, const std::string& aValue)
        : m_Params(aParams)
        , m_Client(m_Params.addr)
        , m_Value(aValue)
        {
            init();
        }

        std::string lastError() const
        {
            Lock lk(m_Mutex);
            return m_LastError;
        }

        void update(const std::string& aValue)
        {
            Lock lk(m_Mutex);
            m_Value = aValue;
        }

        void start(Threads::Group& aGroup)
        {
            m_Periodic.start(
                aGroup,
                m_Params.period,
                [this]() { refresh(); },
                [this]() { cleanup(); });
        }
    };
} // namespace Etcd