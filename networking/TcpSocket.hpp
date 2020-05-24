#pragma once

#include <netinet/tcp.h>
#include "CoreSocket.hpp"

namespace Tcp
{
    class Socket : public Util::CoreSocket
    {
        void create()
        {
            this->m_Fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (this->m_Fd == -1) { throw Error("fail to create a socket"); }
        }
    public:

        Socket() { create(); }
        explicit Socket(int aFd) { m_Fd = aFd; }
        Socket(Socket&& aSocket) { m_Fd = aSocket.m_Fd; aSocket.m_Fd = -1; }

        void connect(uint32_t aRemote, uint16_t aPort)
        {
            struct sockaddr_in sPeer;
            memset(&sPeer, 0, sizeof(sPeer));
            sPeer.sin_family = AF_INET;
	        sPeer.sin_port = htons(aPort);
	        sPeer.sin_addr.s_addr = aRemote;
            if(::connect(m_Fd, (sockaddr*)&sPeer, sizeof(sPeer)) < 0 && errno != EINPROGRESS)
                throw Error("fail to connect");
        }

        int accept()
        {
            int sFd = ::accept(m_Fd, nullptr, nullptr);
            if (sFd == -1 and errno != EAGAIN)
                throw Error("fail to accept");
            return sFd;
        }

        void listen(int aQueue = 128)
        {
            if (::listen(m_Fd, aQueue) == -1)
                throw Error("fail to listen");
        }

        ssize_t read(void* aPtr, ssize_t aSize)
        {
            return checkCall([&](){ return ::recv(m_Fd, aPtr, aSize, 0); }, "recv");
        }

        ssize_t write(const void* aPtr, ssize_t aSize)
        {
            return checkCall([&](){ return ::send(m_Fd, aPtr, aSize, 0); }, "send");
        }

        void set_quick_ack()
        {
            int sVal = 1;
            if (setsockopt(m_Fd, IPPROTO_TCP, TCP_QUICKACK, &sVal, sizeof(sVal)))
                throw Error("fail to set quick ack");
        }

        void set_cork(int aVal = 1)
        {
            if (setsockopt(m_Fd, IPPROTO_TCP, TCP_CORK, &aVal, sizeof(aVal)))
                throw Error("fail to set cork");
        }

        void set_nodelay(int aVal =1)
        {
            if (setsockopt(m_Fd, IPPROTO_TCP, TCP_NODELAY, &aVal, sizeof(aVal)))
                throw Error("fail to set nodelay");
        }

        void set_defer_accept()
        {
            int sVal = 1;
            if (setsockopt(m_Fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &sVal, sizeof(sVal)))
                throw Error("fail to set defer accept");
        }

        void set_fastopen()
        {
            int sVal = 1;
            if (setsockopt(m_Fd, IPPROTO_TCP, TCP_FASTOPEN, &sVal, sizeof(sVal)) < 0)
                throw Error("fail to set fast open");
        }

        void close() override
        {
            if (m_Fd != -1)
            {
                shutdown(m_Fd, SHUT_RDWR);
                Util::CoreSocket::close();
            }
        }
    };
}