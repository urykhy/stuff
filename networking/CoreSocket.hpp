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

namespace Util {
    struct CoreSocket
    {
        using Error = Exception::ErrnoError;

    private:
        CoreSocket(const CoreSocket&) = delete;
        CoreSocket& operator=(const CoreSocket&) = delete;

    protected:
        int m_Fd = -1;

        template <class T>
        ssize_t checkCall(T t, const char* msg)
        {
            ssize_t sRes = t();
            if (sRes < 0 and errno != EAGAIN)
                throw Error("fail to " + std::string(msg));
            return sRes;
        }

    public:
        CoreSocket() {}

        // call bind(0) to make getsockname return port number
        void bind(uint16_t aPort = 0)
        {
            struct sockaddr_in sAddr;
            memset(&sAddr, 0, sizeof(sAddr));
            sAddr.sin_family      = AF_INET;
            sAddr.sin_port        = htons(aPort);
            sAddr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (::bind(m_Fd, (struct sockaddr*)&sAddr, sizeof(sAddr)))
                throw Error("fail to bind");
        }

        virtual void close()
        {
            if (m_Fd != -1) {
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

        void set_buffer(int aRcv, int aSnd)
        {
            if (aRcv > 0 and setsockopt(m_Fd, SOL_SOCKET, SO_RCVBUF, &aRcv, sizeof(aRcv)))
                throw Error("fail to set recv buffer size");
            if (aSnd > 0 and setsockopt(m_Fd, SOL_SOCKET, SO_SNDBUF, &aSnd, sizeof(aSnd)))
                throw Error("fail to set send buffer size");
        }

        std::pair<int, int> get_buffer() const
        {
            socklen_t sDummy = sizeof(int);
            int       sRcv   = 0;
            int       sSnd   = 0;

            if (getsockopt(m_Fd, SOL_SOCKET, SO_RCVBUF, &sRcv, &sDummy))
                throw Error("fail to get recv buffer size");
            if (getsockopt(m_Fd, SOL_SOCKET, SO_SNDBUF, &sSnd, &sDummy))
                throw Error("fail to get send buffer size");

            return std::make_pair(sRcv, sSnd);
        }

        int get_error()
        {
            socklen_t sDummy = sizeof(int);
            int       sError = 0;
            if (getsockopt(m_Fd, SOL_SOCKET, SO_ERROR, &sError, &sDummy))
                throw Error("fail to get socket error");
            return sError;
        }

        uint16_t get_port()
        {
            struct sockaddr_in sTmp;
            socklen_t          sLen = sizeof(sTmp);
            if (getsockname(m_Fd, (struct sockaddr*)&sTmp, &sLen) == -1)
                throw Error("fail to get socket addr");
            return ntohs(sTmp.sin_port);
        }

        sockaddr_in get_peer()
        {
            struct sockaddr_in sTmp;
            socklen_t          sLen = sizeof(sTmp);
            if (getpeername(m_Fd, (struct sockaddr*)&sTmp, &sLen) == -1)
                throw Error("fail to get socket peer");
            return sTmp;
        }

        int get_fd() const { return m_Fd; }

        void set_reuse_port()
        {
            int sReuse = 1;
            if (setsockopt(m_Fd, SOL_SOCKET, SO_REUSEPORT, &sReuse, sizeof(sReuse)))
                throw Error("fail to set reuse port");
        }

        void set_nonblocking()
        {
            int sOld = fcntl(m_Fd, F_GETFL);
            if (fcntl(m_Fd, F_SETFL, O_NONBLOCK | sOld))
                throw Error("fail to set nonblocking");
        }

        void set_timeout()
        {
            struct timeval sTimeout
            {
                0, 100 * 1000
            }; // 0.1 sec
            if (setsockopt(m_Fd, SOL_SOCKET, SO_RCVTIMEO, &sTimeout, sizeof(sTimeout)))
                throw Error("fail to set timeout");
            if (setsockopt(m_Fd, SOL_SOCKET, SO_SNDTIMEO, &sTimeout, sizeof(sTimeout)))
                throw Error("fail to set timeout");
        }

        virtual ~CoreSocket() { close(); }
    };
} // namespace Util