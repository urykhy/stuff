#pragma once

#include "HPack.hpp"
#include "Types.hpp"

namespace asio_http::v2 {

    class InputBuf
    {
        enum
        {
            VACUUM_SIZE = 16384
        };
        std::array<char, MAX_STREAM_EXCLUSIVE> m_Data;

        static_assert(VACUUM_SIZE * 2 < MAX_STREAM_EXCLUSIVE);
        static_assert(sizeof(Header) < VACUUM_SIZE);

        size_t m_readAt  = 0;
        size_t m_writeAt = 0;

        size_t writeAvail() const
        {
            return m_Data.size() - m_writeAt;
        }

        size_t readAvail() const
        {
            assert(m_writeAt >= m_readAt);
            return m_writeAt - m_readAt;
        }

        void vacuum()
        {
            const size_t sAvail = readAvail();
            memmove(&m_Data[0], &m_Data[m_readAt], sAvail);
            m_readAt  = 0;
            m_writeAt = sAvail;
        }

    public:
        // append new data
        asio::mutable_buffer buffer()
        {
            if (writeAvail() < VACUUM_SIZE or readAvail() == 0)
                vacuum();
            return asio::mutable_buffer(&m_Data[m_writeAt], writeAvail());
        }
        void push(size_t aSize)
        {
            m_writeAt += aSize;
        }

        // consume data
        bool append(std::string& aStr, size_t aSize)
        {
            size_t sSize = std::min(aSize, readAvail());
            aStr.append(&m_Data[m_readAt], sSize);
            m_readAt += sSize;
            return aSize == sSize;
        }
    };

} // namespace asio_http::v2