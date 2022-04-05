#pragma once

#include <mutex>

#include "Etcd.hpp"

#include <parser/Json.hpp>

namespace Etcd {
    struct Balancer : public std::enable_shared_from_this<Balancer>
    {
        struct Params
        {
            Client::Params addr;
            std::string    prefix;
            int            period = 10;
            std::string    location;
        };

        struct Entry
        {
            std::string key;
            uint64_t    weight = 0;
            std::string location;

            void from_json(const ::Json::Value& aJson)
            {
                Parser::Json::from_object(aJson, "weight", weight);
                Parser::Json::from_object(aJson, "location", location);
            }
        };

        using List = std::vector<Entry>;

    private:
        const Params       m_Params;
        asio::io_service&  m_Service;
        std::atomic<bool>  m_Stop{false};
        asio::steady_timer m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        List               m_State;
        uint64_t           m_TotalWeight;
        std::string        m_LastError;

        using Error = std::runtime_error;

        void read_i(asio::yield_context yield)
        {
            Client   sClient(m_Service, m_Params.addr, yield);
            auto     sList = sClient.list(m_Params.prefix, 0);
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
                Entry sTmp;
                sTmp.key = x.key;
                Parser::Json::from_value(sRoot, sTmp);
                if (!m_Params.location.empty() and m_Params.location != sTmp.location)
                    continue;
                sState.push_back(std::move(sTmp));
                sWeight += sState.back().weight;
            }

            Lock lk(m_Mutex);
            m_State.swap(sState);
            m_TotalWeight = sWeight;
            m_LastError.clear();
            lk.unlock();
        }

        void read(asio::yield_context yield)
        {
            try {
                read_i(yield);
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
        Balancer(asio::io_service& aService, const Params& aParams)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
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

        void start()
        {
            asio::spawn(m_Timer.get_executor(), [this, p = shared_from_this()](asio::yield_context yield) {
                beast::error_code ec;
                while (!m_Stop) {
                    read(yield);
                    m_Timer.expires_from_now(std::chrono::seconds(m_Params.period));
                    m_Timer.async_wait(yield[ec]);
                }
                clear();
            });
        }

        void stop()
        {
            m_Stop = true;
            m_Timer.cancel();
        }
    };
} // namespace Etcd
