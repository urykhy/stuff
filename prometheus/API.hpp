#pragma once

#include <sstream>

#include <asio_http/Router.hpp>
#include <format/List.hpp>
#include <threads/Periodic.hpp>

#include "Common.hpp"
#include "Manager.hpp"

namespace Prometheus {

    inline void configure(asio_http::RouterPtr aRouter)
    {
        namespace http = boost::beast::http;

        aRouter->insert("/metrics", [](const asio_http::Request& aRequest, asio_http::Response& aResponse) {
            if (aRequest.method() != http::verb::get) {
                aResponse.result(http::status::method_not_allowed);
                return;
            }
            for (auto& x : Manager::instance().toPrometheus())
                aResponse.body().append(x).append("\n");
            aResponse.set(http::field::content_type, "text/plain");
            aResponse.result(http::status::ok);
        });
    }

    inline void start(Threads::Group& aGroup, time_t aPeriod = 60)
    {
        static Common            sCommon;
        static Threads::Periodic sUpdater;

        // run with 1/2 period, to switch buckets in Histogramm
        sUpdater.start(aGroup, aPeriod / 2, []() {
            static bool sSend = false;
            Manager::instance().onTimer();
            sSend = !sSend;
        });
    }

} // namespace