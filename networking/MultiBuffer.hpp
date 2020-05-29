#pragma once

#include <string.h>
#include <sys/socket.h>

#include <array>

namespace Util {
    template <unsigned S>
    class MultiBuffer
    {
        std::array<iovec, S>   m_Iovec;
        std::array<mmsghdr, S> m_Hdr;
        unsigned               m_Index = 0;

    public:
        MultiBuffer()
        {
            memset(m_Iovec.data(), 0, sizeof(m_Iovec));
            memset(m_Hdr.data(), 0, sizeof(m_Hdr));
        }
        void append(void* aBuffer, size_t aSize)
        {
            m_Iovec[m_Index].iov_base         = aBuffer;
            m_Iovec[m_Index].iov_len          = aSize;
            m_Hdr[m_Index].msg_hdr.msg_iov    = &m_Iovec[m_Index];
            m_Hdr[m_Index].msg_hdr.msg_iovlen = 1;
            m_Index++;
        }
        unsigned size() const { return m_Index; }

        mmsghdr* buffer() { return m_Hdr.data(); }
        size_t   size(unsigned aIndex) const { return m_Hdr[aIndex].msg_len; }
    };
} // namespace Util