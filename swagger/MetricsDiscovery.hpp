#pragma once

#include <asio_http/Router.hpp>
#include <container/Algorithm.hpp>
#include <etcd/Balancer.hpp>
#include <parser/Json.hpp>
#include <prometheus/Metrics.hpp>

namespace Swagger {

    /*
    export swagger services for https://prometheus.io/docs/prometheus/latest/http_sd/

    prometheus.yml:

    scrape_configs:
    - job_name: 'swagger'
      http_sd_configs:
      - url: http://127.0.0.1:3000/metrics_discovery
        refresh_interval: 10s
    */

    struct MetricsDiscovery : public std::enable_shared_from_this<MetricsDiscovery>
    {
        struct Entry
        {
            std::string addr;

            bool operator<(const Entry& aOther) const { return addr < aOther.addr; }
            bool operator==(const Entry& aOther) const { return addr == aOther.addr; }

            Format::Json::Value to_json() const
            {
                using namespace Format::Json;

                Value sTargets(::Json::arrayValue);
                sTargets.append(to_value(addr));

                Value sValue(::Json::objectValue);
                sValue["targets"] = std::move(sTargets);
                return sValue;
            }
        };

        using List = std::vector<Entry>;

    private:
        const Etcd::Balancer::Params m_Params;
        boost::asio::io_service&     m_Service;
        std::atomic<bool>            m_Stop{false};
        boost::asio::steady_timer    m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex    m_Mutex;
        List                  m_List;
        std::string           m_LastError;
        Prometheus::Counter<> m_Status;

        void on_timer_i(boost::asio::yield_context yield)
        {
            Etcd::Client sClient(m_Service, m_Params.addr, yield);
            auto         sEtcd = sClient.list(m_Params.prefix, 0, true /* keys only */);
            List         sList;

            for (auto&& x : sEtcd) {
                // x.key like <m_Params.prefix>discovery/swagger/api:discovery/version:1.0/127.0.0.1:3000
                std::string_view sKey = x.key;
                size_t           sPos = sKey.rfind('/');
                if (sPos == std::string_view::npos)
                    continue;
                Entry sTmp;
                sTmp.addr = sKey.substr(sPos + 1);
                sList.push_back(std::move(sTmp));
            }

            Container::sort_unique(sList);

            Lock lk(m_Mutex);
            m_List.swap(sList);
            m_LastError.clear();
            m_Status.set(1);
        }
        void on_timer(boost::asio::yield_context yield)
        {
            try {
                on_timer_i(yield);
            } catch (const std::exception& e) {
                Lock lk(m_Mutex);
                m_LastError = e.what();
                m_Status.set(0);
            }
        }

    public:
        MetricsDiscovery(boost::asio::io_service& aService, const Etcd::Balancer::Params& aParams)
        : m_Params(aParams)
        , m_Service(aService)
        , m_Timer(aService)
        , m_Status("metrics_discovery_up")
        {
        }

        void start()
        {
            boost::asio::spawn(m_Timer.get_executor(), [this, p = shared_from_this()](boost::asio::yield_context yield) {
                boost::beast::error_code ec;
                while (!m_Stop) {
                    on_timer(yield);
                    m_Timer.expires_from_now(std::chrono::seconds(m_Params.period));
                    m_Timer.async_wait(yield[ec]);
                }
            });
        }

        void stop()
        {
            m_Stop = true;
            m_Timer.cancel();
        }

        std::string lastError() const
        {
            Lock lk(m_Mutex);
            return m_LastError;
        }

        std::string to_string() const
        {
            Lock lk(m_Mutex);
            return Format::Json::to_string(Format::Json::to_value(m_List), false /* no indent */);
        }

        void configure(asio_http::RouterPtr aRouter)
        {
            namespace http = boost::beast::http;

            aRouter->insert("/metrics_discovery", [p = shared_from_this()](asio_http::asio::io_service&, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
                if (aRequest.method() != http::verb::get) {
                    aResponse.result(http::status::method_not_allowed);
                    return;
                }
                aResponse.body() = p->to_string();
                aResponse.set(http::field::content_type, "application/json");
                aResponse.result(http::status::ok);
            });
        }
    };
} // namespace Swagger
