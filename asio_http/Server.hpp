#pragma once

// based on https://www.boost.org/doc/libs/master/libs/beast/example/http/server/small/http_server_small.cpp
//          https://www.boost.org/doc/libs/master/libs/beast/example/http/server/coro/http_server_coro.cpp

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <threads/Asio.hpp>
#include "Router.hpp"

namespace asio_http
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    using tcp = boost::asio::ip::tcp;

    inline void session(beast::tcp_stream& aStream, RouterPtr aRouter, net::yield_context yield)
    {
        beast::error_code  ec;
        beast::flat_buffer sBuffer;

        while(true)
        {
            aStream.expires_after(std::chrono::seconds(30));
            Request sRequest;
            http::async_read(aStream, sBuffer, sRequest, yield[ec]);
            if (ec)
                break;

            Response sResponse;
            aRouter->call(sRequest, sResponse);
            sResponse.set(http::field::content_length, sResponse.body().size());
            sResponse.set(http::field::server, "Beast/cxx");

            http::async_write(aStream, sResponse, yield[ec]);
            if (ec)
                break;
            if (sResponse.need_eof()) // we should close the connection, the response indicated the "Connection: close"
                break;
        }
        aStream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    inline void server(std::shared_ptr<tcp::acceptor> aAcceptor, std::shared_ptr<tcp::socket> aSocket, RouterPtr aRouter)
    {
        aAcceptor->async_accept(*aSocket, [=](beast::error_code ec)
        {
            if (!ec)
            {
                aSocket->set_option(tcp::no_delay(true));
                boost::asio::spawn(aAcceptor->get_executor(), [sStream = beast::tcp_stream(std::move(*aSocket)), aRouter] (boost::asio::yield_context yield) mutable {
                    session(sStream, aRouter, yield);
                });
            }
            server(aAcceptor, aSocket, aRouter);
        });
    }

    void startServer(Threads::Asio& aContext, uint16_t aPort, RouterPtr aRouter)
    {
        auto const sAddress = net::ip::make_address("0.0.0.0");
        auto sAcceptor = std::make_shared<tcp::acceptor>(aContext.service(), tcp::endpoint(sAddress, aPort));
        auto sSocket = std::make_shared<tcp::socket>(aContext.service());
        server(sAcceptor, sSocket, aRouter);
    }
}