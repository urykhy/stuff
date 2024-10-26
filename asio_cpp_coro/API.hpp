#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>

namespace AsioHttp {
    namespace ba   = boost::asio;
    namespace bb   = boost::beast;
    namespace http = bb::http;

    using BeastRequest  = http::request<http::string_body>;
    using BeastResponse = http::response<http::string_body>;
    using Headers       = std::unordered_map<std::string, std::string>;

    struct Request
    {
        std::string method;
        std::string url;
        Headers     headers = {};
        std::string body    = {};
    };

    struct Response
    {
        uint16_t    status;
        Headers     headers;
        std::string body;
    };

    struct ClientParams
    {
        bool     alive           = false;
        uint16_t connect_timeout = 100;  // ms
        uint16_t total_timeout   = 1000; // ms
    };

    struct IClient
    {
        virtual ba::awaitable<Response> perform(Request&& aRequest) = 0;
        virtual ~IClient()                                          = default;
    };
    using ClientPtr = std::shared_ptr<IClient>;
    ClientPtr createClient(const ClientParams& aParams);

    struct ServerParams
    {
        uint16_t port         = 3080;
        uint16_t idle_timeout = 10000; // ms
    };
    struct IServer
    {
        using Handler = std::function<ba::awaitable<BeastResponse>(BeastRequest&&)>;

        virtual void addHandler(const std::string& aPrefix, Handler aHandler) = 0;

        virtual ba::awaitable<void> run() = 0;

        virtual ~IServer() = default;
    };
    using ServerPtr = std::shared_ptr<IServer>;
    ServerPtr createServer(const ServerParams& aParams);

} // namespace AsioHttp
