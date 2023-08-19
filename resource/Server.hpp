#pragma once

#include "Tar.hpp"

#include <asio_http/Server.hpp>

namespace resource {
    class Server
    {
        const std::string m_Path;
        const Tar         m_Tar;

        void serve(const asio_http::Request& aRequest, asio_http::Response& aResponse)
        {
            namespace http = boost::beast::http;

            try {
                auto sUrl = aRequest.target();
                sUrl.remove_prefix(m_Path.size());
                std::string sName(sUrl);
                if (sName.empty())
                    sName = "index.html";
                auto sData = m_Tar.get(sName);
                aResponse.body().append(sData);
            } catch (const std::invalid_argument& e) {
                aResponse.result(http::status::not_found);
            } catch (...) {
                aResponse.result(http::status::internal_server_error);
            }
        }

    public:
        Server(const std::string& aPath, const std::string_view aTarData)
        : m_Path(aPath)
        , m_Tar(aTarData)
        {
        }

        void configure(asio_http::RouterPtr aRouter)
        {
            namespace http = boost::beast::http;

            aRouter->insert(
                m_Path,
                [this](asio_http::asio::io_service& aService, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {
                    switch (aRequest.method()) {
                    case http::verb::get:
                        serve(aRequest, aResponse);
                        break;
                    default: aResponse.result(http::status::method_not_allowed);
                    }
                },
                true /* prefix match */);
        }
    };
} // namespace resource
