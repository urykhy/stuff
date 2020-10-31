#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>

#include <threads/Asio.hpp>
#include "Packet.hpp"

namespace asio_mysql
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace net = boost::asio;
    using tcp = boost::asio::ip::tcp;

    struct ImplFace
    {
        virtual beast::error_code process(beast::tcp_stream& aStream,
                                          net::yield_context yield,
                                          const std::string& aQuery) = 0;
        virtual ~ImplFace() {}
    };
    using ImplPtr = std::shared_ptr<ImplFace>;

    inline void session(beast::tcp_stream& aStream, ImplPtr aImpl, net::yield_context yield)
    {
        beast::error_code  ec;
        Container::binary sBuffer;
        omemstream sStream;

        aStream.expires_after(std::chrono::seconds(2));

        // server greeting
        Handshake10 sHandshake;
        sHandshake.serialize(sStream);
        ec = write(aStream, yield, sStream.str());
        if (ec)
            return;

        // read client response
        HandshakeResponse sResponse;
        ec = read(aStream, yield ,sBuffer);
        if (ec)
            return;
        sResponse.parse(sBuffer);

        // send ok packet
        OkResponse sOk;
        sOk.serialize(sStream);
        ec = write(aStream, yield, sStream.str());
        if (ec)
            return;

        while(true)
        {
            aStream.expires_after(std::chrono::seconds(600));

            ec = read(aStream, yield ,sBuffer);
            if (ec)
                break;

            Command sCmd;
            sCmd.parse(sBuffer);

            if (sCmd.command == CMD_QUIT) {
                break;
            } else if (sCmd.command == CMD_QUERY) {
                ec = aImpl->process(aStream, yield, sCmd.query);
            } else {
                BOOST_TEST_MESSAGE("unknown command " << (int)sCmd.command);
                break;
            }
            if (ec)
                break;
        }
        aStream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    inline void server(std::shared_ptr<tcp::acceptor> aAcceptor, std::shared_ptr<tcp::socket> aSocket, ImplPtr aImpl)
    {
        aAcceptor->async_accept(*aSocket, [=](beast::error_code ec)
        {
            if (!ec)
            {
                aSocket->set_option(tcp::no_delay(true));
                boost::asio::spawn(aAcceptor->get_executor(), [sStream = beast::tcp_stream(std::move(*aSocket)), aImpl] (boost::asio::yield_context yield) mutable {
                    session(sStream, aImpl, yield);
                });
            }
            server(aAcceptor, aSocket, aImpl);
        });
    }

    void startServer(Threads::Asio& aContext, uint16_t aPort, ImplPtr aImpl)
    {
        auto const sAddress = net::ip::make_address("0.0.0.0");
        auto sAcceptor = std::make_shared<tcp::acceptor>(aContext.service(), tcp::endpoint(sAddress, aPort));
        auto sSocket = std::make_shared<tcp::socket>(aContext.service());
        server(sAcceptor, sSocket, aImpl);
    }
}