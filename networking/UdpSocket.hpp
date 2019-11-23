#pragma once

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <exception/Error.hpp>

namespace Udp
{
    struct ErrnoError : std::runtime_error { ErrnoError (const std::string& aMsg) : std::runtime_error(Exception::with_errno(aMsg, errno)) {}};

    class Socket
    {
        int m_Fd = -1;
        struct sockaddr_in m_Peer;

        void create()
        {
            m_Fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (m_Fd == -1) { throw ErrnoError("fail to create a socket"); }

            memset(&m_Peer, 0, sizeof(m_Peer));
        }
    public:

        Socket() { create(); }

        Socket(uint16_t aPort)
        {
            create();

            struct sockaddr_in sAddr;
            memset(&sAddr, 0, sizeof(sAddr));
            sAddr.sin_family = AF_INET;
            sAddr.sin_port = htons(aPort);
            sAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(m_Fd, (struct sockaddr*)&sAddr, sizeof(sAddr)))
                throw ErrnoError("fail to bind");
        }

        Socket(uint32_t aRemote, uint16_t aPort)
        {
            create();
            m_Peer.sin_family = AF_INET;
	        m_Peer.sin_port = htons(aPort);
	        m_Peer.sin_addr.s_addr = aRemote;
        }

        void set_nonblocking()
        {
            if (fcntl(m_Fd, F_SETFL, O_NONBLOCK))
                throw ErrnoError("fail to set nonblock");
        }

        void set_timeout()
        {
            struct timeval sTimeout{0, 100 * 1000}; // 0.1 sec
            if (setsockopt(m_Fd, SOL_SOCKET, SO_RCVTIMEO, &sTimeout, sizeof(sTimeout)) < 0)
                throw ErrnoError("fail to set timeout");
            if (setsockopt(m_Fd, SOL_SOCKET, SO_SNDTIMEO, &sTimeout, sizeof(sTimeout)) < 0)
                throw ErrnoError("fail to set timeout");
        }

        ssize_t read(void* aPtr, ssize_t aSize)
        {
            struct sockaddr_in sAddr;
            socklen_t sLen = sizeof(sAddr);
            return ::recvfrom(m_Fd, aPtr, aSize, MSG_TRUNC, (struct sockaddr *)&sAddr, &sLen);
        }

        ssize_t write(const void* aPtr, ssize_t aSize)
        {
            socklen_t sLen = sizeof(m_Peer);
            return ::sendto(m_Fd, aPtr, aSize, 0, (struct sockaddr *)&m_Peer, sLen);
        }

        void close()
        {
            if (m_Fd != -1)
            {
                ::close(m_Fd);
                m_Fd = -1;
            }
        }

        ~Socket() { close(); }
    };
}