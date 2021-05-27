#pragma once

// based on https://www.boost.org/doc/libs/master/libs/beast/example/http/server/small/http_server_small.cpp
//          https://www.boost.org/doc/libs/master/libs/beast/example/http/server/coro/http_server_coro.cpp

#include <boost/asio/spawn.hpp>

#include "Router.hpp"

#include <container/Session.hpp>
#include <threads/Asio.hpp>

namespace asio_http {

    inline void session(asio::io_service& aService, beast::tcp_stream& aStream, RouterPtr aRouter, asio::yield_context yield)
    {
        beast::error_code  ec;
        beast::flat_buffer sBuffer;

        while (true) {
            aStream.expires_after(std::chrono::seconds(30));
            Request sRequest;
            http::async_read(aStream, sBuffer, sRequest, yield[ec]);
            if (ec)
                break;

            Container::Session::Set sPeer("peer", aStream.socket().remote_endpoint().address().to_string());
            Response sResponse;
            aRouter->call(aService, sRequest, sResponse, yield[ec]);
            if (ec)
                break;

            sResponse.prepare_payload();
            if (0 == sResponse.count(http::field::server))
                sResponse.set(http::field::server, "Beast/cxx");

            http::async_write(aStream, sResponse, yield[ec]);
            if (ec)
                break;
            if (sResponse.need_eof()) // we should close the connection, the response indicated the "Connection: close"
                break;
        }
        aStream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    inline void server(asio::io_service& aService, std::shared_ptr<tcp::acceptor> aAcceptor, std::shared_ptr<tcp::socket> aSocket, RouterPtr aRouter)
    {
        aAcceptor->async_accept(*aSocket, [aService = std::ref(aService), aAcceptor, aSocket, aRouter](beast::error_code ec) {
            if (!ec) {
                aSocket->set_option(tcp::no_delay(true));
                boost::asio::spawn(aAcceptor->get_executor(), [aService, sStream = beast::tcp_stream(std::move(*aSocket)), aRouter](boost::asio::yield_context yield) mutable {
                    session(aService, sStream, aRouter, yield);
                });
            }
            server(aService, aAcceptor, aSocket, aRouter);
        });
    }

    void startServer(Threads::Asio& aContext, uint16_t aPort, RouterPtr aRouter)
    {
        auto const sAddress  = asio::ip::make_address("0.0.0.0");
        auto       sAcceptor = std::make_shared<tcp::acceptor>(aContext.service(), tcp::endpoint(sAddress, aPort));
        auto       sSocket   = std::make_shared<tcp::socket>(aContext.service());
        server(aContext.service(), sAcceptor, sSocket, aRouter);
    }
} // namespace asio_http
