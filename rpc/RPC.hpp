#pragma once

#include "Library.hpp"
#include <networking/EPoll.hpp>
#include <networking/UdpSocket.hpp>
#include <tnt17/ReplyWaiter.hpp>

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

        int get_fd() override { return m_Socket.get_fd(); }
    };

    class Client : public Util::EPoll::HandlerFace
    {
        Util::EPoll*       m_Service;
        Udp::Socket        m_Socket;
        tnt17::ReplyWaiter m_Waiter;
        uint64_t           m_Serial = 1;
        uint16_t           m_Timeout;
    public:
        using Handler = tnt17::ReplyWaiter::Handler;

        Client(Util::EPoll* aService, uint32_t aRemote, uint16_t aPort, uint16_t aTimeoutMS = 10)
        : m_Service(aService)
        , m_Socket(aRemote, aPort)
        , m_Timeout(aTimeoutMS)
        { }

        // handler must not throw
        void call(const std::string& aName, const std::string& aParams, Handler aHandler)
        {
            m_Service->post([this, aName, aParams, aHandler = std::move(aHandler)](auto) mutable
            {
                const uint64_t sSerial = m_Serial++;
                const std::string sBody = Library::formatCall(sSerial, aName, aParams);
                m_Waiter.insert(sSerial, m_Timeout, std::move(aHandler));
                m_Socket.write(sBody.data(), sBody.size());
            });
        }

        Result on_read() override
        {
            auto sMsg = m_Socket.read();
            if (sMsg.size > 0)
            {
                auto [sSerial, sFuture] = Library::parseResponse(sMsg.message);
                if (sSerial) {
                    m_Waiter.call(sSerial, std::move(sFuture));
                } else {
                    std::exception_ptr sError = nullptr;
                    try { sFuture.get(); } catch(...) { sError = std::current_exception(); }
                    m_Waiter.flush(sError);
                    m_Waiter.reset();
                }
            }
            m_Waiter.on_timer();
            return Result::RETRY;   // emulate timer
        }
        Result on_write() override { return Result::OK; }
        void on_error() override { BOOST_CHECK(false); }

        int get_fd() override { return m_Socket.get_fd(); }
    };
}