#pragma once

#include <asio_http/Router.hpp>
#include <container/Algorithm.hpp>
#include <parser/Json.hpp>
#include <parser/Parser.hpp>
#include <prometheus/Metrics.hpp>
#include <sd/Balancer.hpp>

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
            std::string service;
            std::string location;

            bool operator<(const Entry& aOther) const { return addr < aOther.addr; }
            bool operator==(const Entry& aOther) const { return addr == aOther.addr; }

            // from etcd balancer data
            void from_json(const ::Json::Value& aJson)
            {
                Parser::Json::from_object(aJson, "location", location);
            }

            // to prometheus sd
            Format::Json::Value to_json() const
            {
                using namespace Format::Json;

                Value sLabels(::Json::objectValue);
                if (!service.empty())
                    write(sLabels, "service", service);
                if (!location.empty())
                    write(sLabels, "location", location);

                Value sTargets(::Json::arrayValue);
                sTargets.append(to_value(addr));

                Value sValue(::Json::objectValue);
                sValue["targets"] = std::move(sTargets);
                sValue["labels"]  = std::move(sLabels);
                return sValue;
            }
        };

        using List = std::vector<Entry>;

    private:
        const SD::Balancer::Params m_Params;
        boost::asio::io_service&   m_Service;
        std::atomic<bool>          m_Stop{false};
        boost::asio::steady_timer  m_Timer;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex    m_Mutex;
        List                  m_List;
        std::string           m_LastError;
        Prometheus::Counter<> m_Status;

        void on_timer_i(boost::asio::yield_context yield)
        {
            constexpr std::string_view X_PREFIX = "discovery/swagger/";

            Etcd::Client sClient(m_Service, m_Params.addr, yield);
            auto         sEtcd = sClient.list(m_Params.prefix + std::string(X_PREFIX));
            List         sList;

            for (auto&& x : sEtcd) {
                // x.key like <m_Params.prefix>discovery/swagger/svc/api/version/127.0.0.1:3000
                std::string_view sKey = x.key;
                sKey.remove_prefix(m_Params.prefix.size());
                sKey.remove_prefix(X_PREFIX.size());

                constexpr unsigned X_SIZE = 4; // number of elements in key
                constexpr unsigned X_SVC  = 0; // service position
                constexpr unsigned X_ADDR = 3; // address position

                std::vector<std::string_view> sParts;
                sParts.reserve(X_SIZE);
                Parser::simple(sKey, sParts, '/');
                if (sParts.size() != X_SIZE)
                    continue;

                Entry sTmp;
                sTmp.addr    = sParts[X_ADDR];
                sTmp.service = sParts[X_SVC];

                try {
                    auto sRoot = Parser::Json::parse(x.value);
                    Parser::Json::from_value(sRoot, sTmp);
                } catch (const std::invalid_argument& e) {
                    continue;
                }

                sList.push_back(std::move(sTmp));
            }

            // uniq by addr
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
        MetricsDiscovery(boost::asio::io_service& aService, const SD::Balancer::Params& aParams)
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
