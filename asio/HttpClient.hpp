#pragma once

#include <functional>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

namespace Asio::Http {

    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

    struct Result
    {
        using Body = http::response<http::dynamic_body>;

        std::string action;
        beast::error_code error;
        Body body;

        Result(const std::string& action_, const beast::error_code error_)
        : action(action_), error(error_) {}
        Result(Body&& body_)
        : body(std::move(body_)) {}
    };

    // Performs an HTTP GET and prints the response
    void
    HttpRequest(const std::string& host,
                const std::string& port,
                const std::string& target,
                int version,
                net::io_context& ioc,
                std::function<void(const Result&)> handler,
                net::yield_context yield)
    {
        beast::error_code ec;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.async_resolve(host, port, yield[ec]);
        if (ec)
        {
            handler(Result("resolve", ec));
            return;
        }

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        stream.async_connect(results, yield[ec]);
        if (ec)
        {
            handler(Result("connect", ec));
            return;
        }

        // Set up an HTTP GET request message
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(stream, req, yield[ec]);
        if (ec)
        {
            handler(Result("write", ec));
            return;
        }

        // This buffer is used for reading and must be persisted
        beast::flat_buffer b;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::async_read(stream, b, res, yield[ec]);
        if (ec)
        {
            handler(Result("read", ec));
            return;
        }

        // Gracefully close the socket
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
        {
            handler(Result("shutdown", ec));
            return;
        }

        // If we get here then the connection is closed gracefully
        handler(Result(std::move(res)));
    }
}
