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
    struct ErrnoError : std::runtime_error { ErrnoError (const std::string& aMsg) : std::runtime_error(Exception::with_errno(aMsg, errno)) {}};

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

        Socket(uint16_t aPort, bool aReuse = false)
        {
            create();

            struct sockaddr_in sAddr;
            memset(&sAddr, 0, sizeof(sAddr));
            sAddr.sin_family = AF_INET;
            sAddr.sin_port = htons(aPort);
            sAddr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (aReuse)
            {
                int sReuse = 1;
                if (setsockopt(m_Fd, SOL_SOCKET, SO_REUSEADDR, &sReuse, sizeof(sReuse)))
                    ErrnoError("fail to set reuse port");
            }

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

        // returns the number of messages received
        ssize_t mread(struct mmsghdr* aPtr, ssize_t aSize)
        {
            // timeout is bugged, so just use nonblocking socket
            return recvmmsg(m_Fd, aPtr, aSize, MSG_TRUNC, nullptr);
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
                throw ErrnoError("fail to get ionread");
            return sAvail;
        }

        void setBufSize(int aRcv, int aSnd)
        {
            if (aRcv > 0 and setsockopt(m_Fd, SOL_SOCKET, SO_RCVBUF, &aRcv, sizeof(aRcv)))
                throw ErrnoError("fail to set recv buffer size");
            if (aSnd > 0 and setsockopt(m_Fd, SOL_SOCKET, SO_SNDBUF, &aSnd, sizeof(aSnd)))
                throw ErrnoError("fail to set send buffer size");
        }

        std::pair<int, int> getBufSize() const
        {
            socklen_t sDummy = sizeof(int);
            int sRcv = 0;
            int sSnd = 0;

            if (getsockopt(m_Fd, SOL_SOCKET, SO_RCVBUF, &sRcv, &sDummy))
                throw ErrnoError("fail to get recv buffer size");
            if (getsockopt(m_Fd, SOL_SOCKET, SO_SNDBUF, &sSnd, &sDummy))
                throw ErrnoError("fail to get send buffer size");

            return std::make_pair(sRcv, sSnd);
        }

        int getError()
        {
            socklen_t sDummy = sizeof(int);
            int sError = 0;
            if (getsockopt(m_Fd, SOL_SOCKET, SO_ERROR, &sError, &sDummy))
                throw ErrnoError("fail to get socket error");
            return sError;
        }

        ~Socket() { close(); }
    };
}