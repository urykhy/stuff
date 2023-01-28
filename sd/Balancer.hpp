#pragma once

#include <mutex>

#include <boost/asio/steady_timer.hpp>

#include <etcd/Etcd.hpp>
#include <parser/Json.hpp>
#include <unsorted/Random.hpp>

namespace SD {

    struct PeerStat
    {
        double success_rate = 0;
        double latency      = 0;
    };

    struct PeerStatProvider
    {
        virtual PeerStat peer_stat(const std::string& aPeer) = 0;
        virtual ~PeerStatProvider(){};
    };
    using ProviderPtr = std::shared_ptr<PeerStatProvider>;

    struct Balancer : public std::enable_shared_from_this<Balancer>
    {
        struct Params
        {
            Etcd::Client::Params addr;
            std::string          prefix;
            int                  period = 10;
            std::string          location;
        };

        struct Entry
        {
            std::string key;
            double      weight   = 0;
            uint32_t    threads  = 1;
            std::string location = {};

            void from_json(const ::Json::Value& aJson)
            {
                Parser::Json::from_object(aJson, "weight", weight);
                Parser::Json::from_object(aJson, "threads", threads);
                Parser::Json::from_object(aJson, "location", location);
            }
        };

        using State = std::map<double, Entry>;

    private:
        const Params              m_Params;
        boost::asio::io_service&  m_Service;
        std::atomic<bool>         m_Stop{false};
        boost::asio::steady_timer m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        State              m_State;
        std::string        m_LastError;
        ProviderPtr        m_Provider;

        struct History
        {
            double weight = 0;
        };
        std::map<std::string, History> m_History;

        using Error = std::runtime_error;

        void read_i(boost::asio::yield_context yield)
        {
            Etcd::Client       sClient(m_Service, m_Params.addr, yield);
            auto               sList = sClient.list(m_Params.prefix, 0);
            std::vector<Entry> sState;

            for (auto&& x : sList) {
                x.key.erase(0, m_Params.prefix.size());
                Json::Value sRoot;
                try {
                    sRoot = Parser::Json::parse(x.value);
                } catch (const std::invalid_argument& e) {
                    throw Error(std::string("SD: bad etcd server response: ") + e.what());
                }
                Entry sTmp;
                sTmp.key = x.key;
                Parser::Json::from_value(sRoot, sTmp);
                if (!m_Params.location.empty() and String::starts_with(sTmp.location, m_Params.location))
                    continue;
                sState.push_back(std::move(sTmp));
            }

            update(sState);
        }

        void adjust_weights(std::vector<Entry>& aState)
        {
            const double LOSS_MAX     = 0.5;
            const double LOSS_FACTOR  = 0.90;
            const double BOOST_FACTOR = 1.05;

            for (auto& x : aState) {
                const auto   sStat           = m_Provider->peer_stat(x.key);
                auto&        sHistory        = m_History[x.key];
                const double sPreviousWeight = sHistory.weight > 0 ? sHistory.weight : x.weight;
                double       sTargetWeight   = x.weight;
                if (sStat.latency > x.threads / x.weight /* server latency */) {
                    const double sClientWeight = x.threads / sStat.latency;
                    sTargetWeight              = std::max(sClientWeight, sTargetWeight * LOSS_MAX);
                }
                if (sStat.success_rate < 0.95) {
                    sTargetWeight *= std::max(sStat.success_rate, LOSS_MAX);
                }
                if (sTargetWeight >= sPreviousWeight) {
                    sTargetWeight = std::min(sTargetWeight, sPreviousWeight * BOOST_FACTOR);
                } else {
                    sTargetWeight = std::max(sTargetWeight, sPreviousWeight * LOSS_FACTOR);
                }
                x.weight        = sTargetWeight;
                sHistory.weight = sTargetWeight;
            }
        }

#ifdef BOOST_TEST_MODULE
    public:
#endif
        // change aState
        void
        update(std::vector<Entry>& aState)
        {
            Lock lk(m_Mutex);
            if (m_Provider)
                adjust_weights(aState);

            const double sTotalWeight = [&aState]() {
                double sSum = 0;
                for (auto& x : aState)
                    sSum += x.weight;
                return sSum;
            }();

            m_State.clear();
            double sWeight = 0;
            for (auto& x : aState) {
                sWeight += (x.weight / sTotalWeight);
                m_State[sWeight] = x;
            }
            m_LastError.clear();
        }

    private:
        void read(boost::asio::yield_context yield)
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
        }

    public:
        Balancer(boost::asio::io_service& aService, const Params& aParams)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
        {
        }

        void with_peer_stat(ProviderPtr aProvider)
        {
            Lock lk(m_Mutex);
            m_Provider = aProvider;
        }

        State state() const
        {
            Lock lk(m_Mutex);
            return m_State;
        }

        Entry random()
        {
            Lock lk(m_Mutex);
            if (m_State.empty())
                throw Error("SD: no peers available");
            auto sIt = m_State.lower_bound(Util::drand48());
            if (sIt != m_State.end()) {
                return sIt->second;
            } else {
                return m_State.rbegin()->second;
            }
        }

        std::string lastError() const
        {
            Lock lk(m_Mutex);
            return m_LastError;
        }

        void start()
        {
            boost::asio::spawn(m_Timer.get_executor(), [this, p = shared_from_this()](boost::asio::yield_context yield) {
                boost::beast::error_code ec;
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
} // namespace SD
