#include <cassert>
#include <iostream>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#define BOOST_TEST_MODULE
#include "Balancer.hpp"
#include "NotifyWeight.hpp"

#include <prometheus/Manager.hpp>

struct ServerSettings
{
    double latency      = 0.1;
    double net_latency  = 0.0001;
    double success_rate = 1.0;
};

static const double MAX_LATENCY = 0.5;

struct AvoidEtcd : public SD::NotifyFace
{
    std::string  m_Data;
    virtual void start() {}
    virtual void update(const std::string& aValue)
    {
        m_Data = aValue;
    }
    void clear()
    {
        m_Data.clear();
    }
};

struct Server
{
    const ServerSettings              m_Settings;
    std::shared_ptr<AvoidEtcd>        m_Etcd;
    std::shared_ptr<SD::NotifyWeight> m_Notify;
    uint32_t                          m_Thread = 0;
    std::vector<double>               m_Offset;

    Server(Threads::Asio& aAsio, const SD::NotifyWeight::Params& aParams, const ServerSettings& aSettings)
    : m_Settings(aSettings)
    {
        m_Etcd   = std::make_shared<AvoidEtcd>();
        m_Notify = std::make_shared<SD::NotifyWeight>(aAsio.service(), aParams, m_Etcd);
        m_Offset.resize(aParams.threads);
    }
    bool add(SD::Balancer::PeerInfoPtr aPeerInfo, double aMoment)
    {
        const time_t sNow = std::floor(aMoment);
        double       sELA = m_Settings.latency;

        if (m_Offset[m_Thread] > aMoment) {
            sELA += m_Offset[m_Thread] - aMoment;
        }
        if (sELA >= MAX_LATENCY) {
            aPeerInfo->add(m_Settings.net_latency, sNow, false);
            return false;
        }

        m_Offset[m_Thread]  = aMoment + sELA;
        m_Thread            = (m_Thread + 1) % m_Offset.size();
        const bool sSuccess = Util::drand48() < m_Settings.success_rate;
        m_Notify->add(m_Settings.latency, sNow); // account server time
        aPeerInfo->add(sELA + m_Settings.net_latency, sNow, sSuccess);
        return sSuccess;
    }
};

int main(int argc, char** argv)
{
    // clang-format off
    po::options_description desc("Program options");
    desc.add_options()("help,h", "show usage information")
    ("load", po::value<float>()->default_value(0.5), "load level (1 = 100% of cluster capacity)")
    ("time", po::value<time_t>()->default_value(60), "time to run in seconds");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    // prepare
    Threads::Asio  sAsio;
    Threads::Group sGroup;
    sAsio.start(sGroup);

    using ServerPtr = std::shared_ptr<Server>;
    std::map<std::string, ServerPtr> sServers;

    SD::NotifyWeight::Params sParams;
    ServerSettings           sSettings;
    sServers["normal"]     = std::make_shared<Server>(sAsio, sParams, sSettings);
    sSettings.latency      = 0.2;
    sServers["slow"]       = std::make_shared<Server>(sAsio, sParams, sSettings);
    sSettings              = {};
    sSettings.net_latency  = 0.1;
    sServers["bad_net"]    = std::make_shared<Server>(sAsio, sParams, sSettings);
    sSettings              = {};
    sSettings.success_rate = 0.4;
    sServers["faulty"]     = std::make_shared<Server>(sAsio, sParams, sSettings);
    sSettings              = {};
    sParams.threads        = 2;
    sServers["large"]      = std::make_shared<Server>(sAsio, sParams, sSettings);
    sParams                = {};
    sSettings.latency      = 0.05;
    sServers["fast"]       = std::make_shared<Server>(sAsio, sParams, sSettings);
    sSettings              = {};

    SD::Balancer::Params sBalancerParams;
    sBalancerParams.use_server_rps = true;
    auto sBalancer                 = std::make_shared<SD::Balancer::Engine>(sAsio.service(), sBalancerParams);

    auto sRefresh = [&]() {
        std::vector<SD::Balancer::Entry> sState;
        for (auto& [sName, sServer] : sServers) {
            auto                sValue = Parser::Json::parse(sServer->m_Etcd->m_Data);
            SD::Balancer::Entry sTmp;
            sTmp.key = sName;
            Parser::Json::from_value(sValue, sTmp);
            sState.push_back(std::move(sTmp));
        }
        if (!sState.empty())
            sBalancer->update(std::move(sState));
    };
    sRefresh();

    const uint32_t TOTAL_WEIGHT  = sBalancer->m_State.m_TotalWeight;
    const int      REQUEST_COUNT = TOTAL_WEIGHT * vm["load"].as<float>();

    // csv header
    std::cout << "ts" << ',';
    for (auto& x : sServers)
        std::cout << x.first << ',';
    std::cout << "failed" << std::endl;

    // put load
    for (time_t sTime = 0; sTime <= vm["time"].as<time_t>(); sTime++) {
        std::map<std::string, uint32_t> sSuccess;
        uint32_t                        sFailed = 0;
        for (auto& x : sServers)
            sSuccess[x.first] = 0;

        for (int i = 0; i < REQUEST_COUNT; i++) {
            const double sMoment = sTime + (double)i / REQUEST_COUNT;
            try {
                auto sPeerInfo = sBalancer->random(sTime);
                auto sIt       = sServers.find(sPeerInfo->key());
                assert(sIt != sServers.end());
                if (sIt->second->add(sPeerInfo, sMoment))
                    sSuccess[sPeerInfo->key()]++;
                else
                    sFailed++;
            } catch (const SD::Balancer::Error& e) {
                sFailed++;
            }
        }
        sRefresh();
        std::cout << sTime << ',';
        for (auto& x : sSuccess)
            std::cout << x.second << ',';
        std::cout << sFailed << std::endl;
    }

    // dump etcd state
    for (auto& [sName, sServer] : sServers)
        std::cerr << sName << " " << sServer->m_Etcd->m_Data << std::endl;

    // dump metrics
    for (auto& x : Prometheus::Manager::instance().toPrometheus())
        std::cerr << x << std::endl;

    return 0;
}