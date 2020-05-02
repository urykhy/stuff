#pragma once

#include <sstream>

#include <format/List.hpp>
#include "Manager.hpp"
#include "Common.hpp"
#include <threads/Periodic.hpp>
#include <networking/Resolve.hpp>
#include <networking/UdpSocket.hpp>
#include <asio_http/Router.hpp>

namespace Stat
{
    inline void prometheusRouter(asio_http::RouterPtr aRouter)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        aRouter->insert("/metrics", [](const asio_http::Request& aRequest, asio_http::Response& aResponse)
        {
            if (aRequest.method() != http::verb::get)
            {
                aResponse.result(http::status::method_not_allowed);
                return;
            }

            aResponse.result(http::status::ok);
            aResponse.set(http::field::content_type, "text/plain");
            for (auto& x : Manager::instance().toPrometheus())
                aResponse.body().append(x).append("\n");
        });
    }

    struct Config
    {
        time_t      period = 60;

        // graphite params. empty host = graphite disabled
        std::string host;
        uint16_t    port = 2003;
        std::string prefix;
    };

    inline void pushGraphite(const Config& aConfig)
    {
        const uint16_t MAX_SIZE = 1400; // udp packet limit

        std::string sPacket;
        Udp::Socket sSocket(Util::resolveName(aConfig.host), aConfig.port);
        for (auto& x: Manager::instance().toGraphite())
        {
            std::string sRow = (aConfig.prefix.empty() ? "" : aConfig.prefix + ".") + x + "\n";
            if (sPacket.size() + sRow.size() > MAX_SIZE)
            {
                sSocket.write(sPacket);
                sPacket.clear();
            }
            sPacket += sRow;
        }
        sSocket.write(sPacket);
    }

    inline void start(Threads::Group& aGroup, const Config& aConfig)
    {
        static Common sCommon;
        static Threads::Periodic sUpdater;

        // run with 1/2 period, to switch buckets in Histogramm
        sUpdater.start(aGroup, aConfig.period / 2, [&aConfig](){
            static bool sSend = false;
            Manager::instance().onTimer();
            if (sSend and !aConfig.host.empty())
                pushGraphite(aConfig);
            sSend = !sSend;
        });
    }

} // namespace Stat