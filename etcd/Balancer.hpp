#pragma once

#include <mutex>

#include "Etcd.hpp"

#include <parser/Json.hpp>
#include <threads/Periodic.hpp>

namespace Etcd {
    struct Balancer
    {
        struct Params
        {
            Client::Params addr;
            std::string    prefix;
            int            period = 60;
        };

        struct Entry
        {
            std::string key;
            uint64_t    weight = 0;
        };

        using List = std::vector<Entry>;

    private:
        const Params      m_Params;
        Etcd::Client      m_Client;
        Threads::Periodic m_Periodic;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        List               m_State;
        uint64_t           m_TotalWeight;
        std::string        m_LastError;

        using Error = std::runtime_error;

        void read_i()
        {
            auto     sList = m_Client.list(m_Params.prefix);
            List     sState;
            uint64_t sWeight = 0;

            for (auto&& x : sList) {
                x.key.erase(0, m_Params.prefix.size());
                Json::Value sRoot;
                try {
                    sRoot = Parser::Json::parse(x.value);
                } catch (const std::invalid_argument& e) {
                    throw Error(std::string("etcd: bad server response: ") + e.what());
                }
                if (sRoot.isObject() and sRoot.isMember("weight") and sRoot["weight"].isUInt64()) {
                    sState.push_back({x.key, sRoot["weight"].asUInt64()});
                    sWeight += sState.back().weight;
                }
            }

            Lock lk(m_Mutex);
            m_State.swap(sState);
            m_TotalWeight = sWeight;
            m_LastError.clear();
            lk.unlock();
        }

        void read()
        {
            try {
                read_i();
            } catch (const std::exception& e) {
                Lock lk(m_Mutex);
                m_LastError = e.what();
            }
        }

        void clear()
        {
            Lock lk(m_Mutex);
            m_State.clear();
            m_TotalWeight = 0;
        }

    public:
        Balancer(const Params& aParams)
        : m_Params(aParams)
        , m_Client(m_Params.addr)
        {}

        List state() const
        {
            Lock lk(m_Mutex);
            return m_State;
        }

        Entry random()
        {
            Lock     lk(m_Mutex);
            uint64_t sKey = lrand48() % m_TotalWeight;

            for (const auto& x : m_State)
                if (sKey < x.weight)
                    return x;
                else
                    sKey -= x.weight;

            throw Error("Etcd::Balancer fail to pick item");
        }

        std::string lastError() const
        {
            Lock lk(m_Mutex);
            return m_LastError;
        }

        void start(Threads::Group& aGroup)
        {
            m_Periodic.start(
                aGroup,
                m_Params.period,
                [this]() { read(); },
                [this]() { clear(); });
        }
    };
} // namespace Etcd
