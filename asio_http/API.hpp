#pragma once

#include <future>
#include <string>

#ifdef ASIO_HTTP_LIBRARY_HEADER
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/string_body.hpp>
#else
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#endif

namespace asio_http {

    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
#ifndef ASIO_HTTP_LIBRARY_HEADER
    using tcp = boost::asio::ip::tcp;
#endif

    using Strand   = asio::strand<asio::io_context::executor_type>;
    using Request  = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    namespace Headers {
        static const std::string Accept      = "Accept";
        static const std::string ContentType = "Content-Type";
        static const std::string Host        = "Host";
        static const std::string UserAgent   = "User-Agent";
    } // namespace Headers

    struct Addr
    {
        std::string host;
        std::string port;

        auto as_tuple() const
        {
            return std::tie(host, port);
        }
        bool operator<(const Addr& aOther) const
        {
            return as_tuple() < aOther.as_tuple();
        }
    };

    struct ClientRequest
    {
        struct iless
        {
            bool operator()(const std::string& a, const std::string& b) const
            {
                return strcasecmp(a.data(), b.data()) < 0;
            }
        };
        using Headers = std::map<std::string, std::string, iless>;

        http::verb  method = http::verb::get;
        std::string url;
        std::string body    = {};
        Headers     headers = {};

        // not used by http2
        time_t connect = 100;  // timeout in ms
        time_t total   = 3000; // timeout in ms
    };

    using Promise = std::shared_ptr<std::promise<Response>>;

    struct Client
    {
        virtual std::future<Response> async(ClientRequest&& aRequest)                              = 0;
        virtual std::future<Response> async_y(ClientRequest&& aRequest, asio::yield_context yield) = 0;
        virtual ~Client(){};
    };
    using ClientPtr = std::shared_ptr<Client>;

} // namespace asio_http