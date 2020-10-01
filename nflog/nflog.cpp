#include "NFlog.hpp"

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <iostream>

#include <boost/program_options.hpp>

#include <networking/Resolve.hpp>
#include <unsorted/Daemon.hpp>
#include <unsorted/Log4cxx.hpp>

namespace po = boost::program_options;

volatile sig_atomic_t gStopFlag = 0;

static void signalHandler(int signo)
{
    gStopFlag = 1;
}

struct Packet
{
    using Error = std::invalid_argument;

    std::string str() const
    {
        return m_Str.str();
    }

    void decode(std::string_view aPacket)
    {
        m_Str.str("");

        switch (h_ip(aPacket)) {
        case IPPROTO_TCP: h_tcp(aPacket); break;
        case IPPROTO_UDP: h_udp(aPacket); break;
        case IPPROTO_ICMP: h_icmp(aPacket); break;
        default: break;
        }
    }

private:
    std::stringstream m_Str;

    int h_ip(std::string_view& aPacket)
    {
        if (aPacket.size() < sizeof(iphdr))
            throw Error("too short message to contain ip header");

        auto sHeader = (struct iphdr*)(aPacket.data());
        m_Str << '\t' << Util::formatAddr(sHeader->saddr)
              << '\t' << Util::formatAddr(sHeader->daddr);
        aPacket.remove_prefix(sHeader->ihl * 4);

        return sHeader->protocol;
    }

    int h_tcp(std::string_view& aPacket)
    {
        if (aPacket.size() < sizeof(tcphdr))
            throw Error("too short message to contain tcp header");

        auto sHeader = (struct tcphdr*)(aPacket.data());
        m_Str << '\t' << "TCP"
              << '\t' << ntohs(sHeader->source)
              << '\t' << ntohs(sHeader->dest);
        aPacket.remove_prefix(sHeader->doff * 4);
        return 0;
    }

    int h_udp(std::string_view& aPacket)
    {
        if (aPacket.size() < sizeof(udphdr))
            throw Error("too short message to contain udp header");

        auto sHeader = (struct udphdr*)(aPacket.data());
        m_Str << '\t' << "UDP"
              << '\t' << ntohs(sHeader->source)
              << '\t' << ntohs(sHeader->dest);
        aPacket.remove_prefix(sizeof(*sHeader));
        return 0;
    }

    int h_icmp(std::string_view& aPacket)
    {
        if (aPacket.size() < sizeof(icmphdr))
            throw Error("too short message to contain icmp header");

        auto sHeader = (struct icmphdr*)(aPacket.data());
        m_Str << '\t' << "ICMP"
              << '\t' << (int)sHeader->type
              << '\t' << (int)sHeader->code;
        aPacket.remove_prefix(sizeof(*sHeader));
        return 0;
    }
};

struct Handler
{
    void operator()(struct nflog_data* aData)
    {
        char* sPrefix  = nflog_get_prefix(aData);
        char* sPayload = 0;
        int   sLen     = nflog_get_payload(aData, &sPayload);

        Packet sPacket;
        if (sLen > 0)
            sPacket.decode(std::string_view(sPayload, sLen));

        WARN(Util::interfaceName(nflog_get_indev(aData))
             << '\t' << Util::interfaceName(nflog_get_outdev(aData))
             << '\t' << sLen
             << sPacket.str()
             << '\t' << (sPrefix ? sPrefix : ""));
    }
};

int main(int argc, char** argv)
{
    po::options_description desc("Program options");
    desc.add_options()("help,h", "show usage information")("group", po::value<int>()->default_value(1), "group to listen")("user", po::value<std::string>()->default_value("nobody"), "username to drop privileges");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    Handler sHandler;

    Util::setSignal(SIGTERM, signalHandler);
    Util::setSignal(SIGINT, signalHandler);

    INFO("starting on group " << vm["group"].as<int>());
    NFlog::Manager sManager(vm["group"].as<int>(), sHandler);

    Util::switchUser(vm["user"].as<std::string>());
    INFO("running as " << vm["user"].as<std::string>());

    sManager.loop(gStopFlag);

    INFO("exiting");
}