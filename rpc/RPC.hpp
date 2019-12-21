#pragma once

#include "Library.hpp"
#include <networking/EPoll.hpp>
#include <networking/UdpSocket.hpp>

namespace RPC
{
    class Server : public Util::EPoll::HandlerFace
    {
        Udp::Socket m_Socket;
        const Library& m_Library;
    public:
        Server(uint16_t aPort, const Library& aLibrary)
        : m_Socket(aPort)
        , m_Library(aLibrary)
        {}

        Result on_read() override
        {
            auto sMsg = m_Socket.read();
            auto sResult = m_Library.call(sMsg.message);
            Udp::Socket::Msg sReply{0, sResult, sMsg.addr};
            m_Socket.write(sReply);
            return m_Socket.ionread() ? Result::RETRY : Result::OK;
        }
        Result on_write() override { return Result::OK; }
        void on_error() override { BOOST_CHECK(false); }

        int get() { return m_Socket.get(); }
    };
}