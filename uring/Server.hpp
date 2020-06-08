#pragma once

#include <httpd/Parser.hpp>

#include "URing.hpp"

namespace URing::http {
    class Server : public Util::URingUserPtr
    {
        Util::URing&           m_Ring;
        std::array<char, 4096> m_Input;
        const std::string      m_Output{
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 10\r\n"
            "Content-Type: text/numbers\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "0123456789"};

        httpd::Parser  m_Parser;
        httpd::Request m_Request;

        bool m_HaveRequest{false};
        int  m_FD;

    public:
        Server(Util::URing& aRing, int aFD)
        : m_Ring(aRing)
        , m_Parser([this](httpd::Request& aRequest) {
            m_Request     = std::move(aRequest);
            m_HaveRequest = true;
        })
        , m_FD(aFD)
        {}

        void start()
        {
            m_Ring.read(m_FD, shared_from_base<Server>(), iovec{m_Input.data(), m_Input.size()});
        }

        void on_event(int aKind, int32_t aRes) override
        {
            switch (aKind) {
            case IORING_OP_READ:
                if (aRes > 0) {
                    ssize_t sUsed = m_Parser.consume(m_Input.data(), aRes);
                    if (sUsed != aRes) {
                        //BOOST_TEST_MESSAGE("fail to parse. closing ...");
                        m_Ring.close(m_FD, shared_from_base<Server>());
                        return;
                    }
                    if (m_HaveRequest) {
                        // must route request and so on. bu we just kidding
                        m_Ring.write(m_FD, shared_from_base<Server>(), iovec{const_cast<char*>(&m_Output[0]), m_Output.size()});
                    } else {
                        m_Ring.read(m_FD, shared_from_base<Server>(), iovec{m_Input.data(), m_Input.size()});
                    }
                } else {
                    //BOOST_TEST_MESSAGE("readed 0 bytes. closing ...");
                    m_Ring.close(m_FD, shared_from_base<Server>());
                }
                break;
            case IORING_OP_WRITE:
                if (!m_Request.m_KeepAlive) {
                    //BOOST_TEST_MESSAGE("not keep-alive. closing ...");
                    m_Ring.close(m_FD, shared_from_base<Server>());
                } else {
                    m_Request.clear();
                    m_HaveRequest = false;
                    m_Ring.read(m_FD, shared_from_base<Server>(), iovec{&m_Input[0], m_Input.size()});
                }
                break;
            case IORING_OP_CLOSE:
                //BOOST_TEST_MESSAGE("connection closed");
                break;
            default:
                assert(0);
            }
        }
    };

    struct Listener : public Util::URingUserPtr
    {
        Util::URing& m_Ring;
        int          m_FD;

        Listener(Util::URing& aRing, int aFD)
        : m_Ring(aRing)
        , m_FD(aFD)
        {}

        void on_event(int aKind, int32_t aRes) override
        {
            if (aKind < 0) {
                BOOST_TEST_MESSAGE("listener error: " << aRes);
            } else {
                auto sConn = std::make_shared<Server>(m_Ring, aRes);
                sConn->start();
            }
            m_Ring.accept(m_FD, shared_from_base<Listener>());
        }
    };
} // namespace URing::http