#pragma once

#include <string>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace asio_http {

    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
    using tcp       = boost::asio::ip::tcp;

    using Request  = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    namespace Headers {
        static const std::string Accept      = "Accept";
        static const std::string ContentType = "Content-Type";
        static const std::string Host        = "Host";
        static const std::string UserAgent   = "User-Agent";
    } // namespace Headers

    struct ClientRequest
    {
        using Headers = std::map<std::string, std::string>;

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
        virtual std::future<Response> async(ClientRequest&& aRequest) = 0;
        virtual ~Client(){};
    };
    using ClientPtr = std::shared_ptr<Client>;

} // namespace asio_http