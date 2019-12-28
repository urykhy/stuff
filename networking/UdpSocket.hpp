#pragma once

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <exception/Error.hpp>

namespace Udp
{
    template<unsigned S>
    class MultiBuffer
    {
        std::array<iovec, S> m_Iovec;
        std::array<mmsghdr, S> m_Hdr;
        unsigned m_Index = 0;
    public:

        MultiBuffer()
        {
            memset(m_Iovec.data(), 0, sizeof(m_Iovec));
            memset(m_Hdr.data(),   0, sizeof(m_Hdr));
        }
        void append(void* aBuffer, size_t aSize)
        {
            m_Iovec[m_Index].iov_base = aBuffer;
            m_Iovec[m_Index].iov_len  = aSize;
            m_Hdr[m_Index].msg_hdr.msg_iov = &m_Iovec[m_Index];
            m_Hdr[m_Index].msg_hdr.msg_iovlen = 1;
            m_Index++;
        }
        unsigned size() const { return m_Index; }

        mmsghdr* buffer() { return m_Hdr.data(); }
        size_t size(unsigned aIndex) const { return m_Hdr[aIndex].msg_len; }
    };

    struct Socket
    {
        using Error = Exception::ErrnoError;
    private:
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        int m_Fd = -1;
        struct sockaddr_in m_Peer;

        void create()
        {
            m_Fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (m_Fd == -1) { throw Error("fail to create a socket"); }

            memset(&m_Peer, 0, sizeof(m_Peer));
        }

        // call bind(0) to make getsockname return port number
        void bind(uint16_t aPort = 0)
        {
            struct sockaddr_in sAddr;
            memset(&sAddr, 0, sizeof(sAddr));
            sAddr.sin_family = AF_INET;
            sAddr.sin_port = htons(aPort);
            sAddr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (::bind(m_Fd, (struct sockaddr*)&sAddr, sizeof(sAddr)))
                throw Error("fail to bind");
        }
    public:

        Socket(uint16_t aPort = 0, bool aReuse = false)
        {
            create();

            if (aReuse)
            {
                int sReuse = 1;
                if (setsockopt(m_Fd, SOL_SOCKET, SO_REUSEPORT, &sReuse, sizeof(sReuse)))
                    throw Error("fail to set reuse port");
            }

            bind(aPort);
        }

        Socket(uint32_t aRemote, uint16_t aPort)
        {
            create();
            bind();

            m_Peer.sin_family = AF_INET;
	        m_Peer.sin_port = htons(aPort);
	        m_Peer.sin_addr.s_addr = aRemote;
        }

        void set_nonblocking()
        {
            if (fcntl(m_Fd, F_SETFL, O_NONBLOCK))
                throw Error("fail to set nonblock");
        }

        void set_timeout()
        {
            struct timeval sTimeout{0, 100 * 1000}; // 0.1 sec
            if (setsockopt(m_Fd, SOL_SOCKET, SO_RCVTIMEO, &sTimeout, sizeof(sTimeout)) < 0)
                throw Error("fail to set timeout");
            if (setsockopt(m_Fd, SOL_SOCKET, SO_SNDTIMEO, &sTimeout, sizeof(sTimeout)) < 0)
                throw Error("fail to set timeout");
        }

        struct Msg
        {
            ssize_t size = 0;
            std::string message;
            struct sockaddr_in addr;

            // remote in network byte order. port - in host order
            void set_peer(uint32_t aRemote, uint16_t aPort)
            {
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
	            addr.sin_port = htons(aPort);
	            addr.sin_addr.s_addr = aRemote;
            }
        };

        Msg read()
        {
            Msg sResult;
            auto sSize = ionread();
            if (sSize > 0)
            {
                socklen_t sLen = sizeof(sResult.addr);
                sResult.message.resize(sSize);
                sResult.size = recvfrom(m_Fd, sResult.message.data(), sSize, MSG_TRUNC, (struct sockaddr *)&sResult.addr, &sLen);
            }
            return sResult;
        }

        ssize_t read(void* aPtr, ssize_t aSize)
        {
            struct sockaddr_in sAddr;
            socklen_t sLen = sizeof(sAddr);
            return ::recvfrom(m_Fd, aPtr, aSize, MSG_TRUNC, (struct sockaddr *)&sAddr, &sLen);
        }

        // aCount - max number of messages to read
        // returns the number of messages received
        ssize_t read(struct mmsghdr* aPtr, ssize_t aCount)
        {
            // timeout is bugged, so just use nonblocking socket
            return recvmmsg(m_Fd, aPtr, aCount, MSG_TRUNC, nullptr);
        }

        ssize_t write(const Msg& aMessage)
        {
            socklen_t sLen = sizeof(aMessage.addr);
            return ::sendto(m_Fd, aMessage.message.data(), aMessage.message.size(), 0, (struct sockaddr *)&aMessage.addr, sLen);
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

        // FIONREAD on a UDP socket returns the size of the first datagram.
        ssize_t ionread()
        {
            int sAvail = 0;
            if (ioctl(m_Fd, FIONREAD, &sAvail))
                throw Error("fail to get ionread");
            return sAvail;
        }

        // Get the number of bytes in the output buffer
        ssize_t outq()
        {
            int sSize = 0;
            if (ioctl(m_Fd, TIOCOUTQ, &sSize))
                throw Error("fail to get tiocoutq");
            return sSize;
        }

        void setBufSize(int aRcv, int aSnd)
        {
            if (aRcv > 0 and setsockopt(m_Fd, SOL_SOCKET, SO_RCVBUF, &aRcv, sizeof(aRcv)))
                throw Error("fail to set recv buffer size");
            if (aSnd > 0 and setsockopt(m_Fd, SOL_SOCKET, SO_SNDBUF, &aSnd, sizeof(aSnd)))
                throw Error("fail to set send buffer size");
        }

        std::pair<int, int> getBufSize() const
        {
            socklen_t sDummy = sizeof(int);
            int sRcv = 0;
            int sSnd = 0;

            if (getsockopt(m_Fd, SOL_SOCKET, SO_RCVBUF, &sRcv, &sDummy))
                throw Error("fail to get recv buffer size");
            if (getsockopt(m_Fd, SOL_SOCKET, SO_SNDBUF, &sSnd, &sDummy))
                throw Error("fail to get send buffer size");

            return std::make_pair(sRcv, sSnd);
        }

        int getError()
        {
            socklen_t sDummy = sizeof(int);
            int sError = 0;
            if (getsockopt(m_Fd, SOL_SOCKET, SO_ERROR, &sError, &sDummy))
                throw Error("fail to get socket error");
            return sError;
        }

        uint16_t port()
        {
            struct sockaddr_in sTmp;
            socklen_t sLen = sizeof(sTmp);
            if (getsockname(m_Fd, (struct sockaddr *)&sTmp, &sLen) == -1)
                throw Error("fail to get socket addr");
            return ntohs(sTmp.sin_port);
        }

        int get() { return m_Fd; }

        ~Socket() { close(); }
    };
}