#pragma once
#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>

namespace tnt17
{
    namespace asio = boost::asio;
    using boost::asio::ip::tcp;

    struct Message
    {
        struct Header {
            unsigned char next = 0;
            uint32_t      len = 0;
            size_t decode() const { assert(next == 0xce); return be32toh(len); }
        } __attribute__ ((packed));

        Message() {}
        Message(const std::string& aData)
        {
            m_Header.next = 0xce;
            m_Header.len  = htobe32(aData.size());
            m_Body = aData;
        }

        using const_buffer = boost::asio::const_buffer;
        using BufferList   = std::array<const_buffer, 2>;
        BufferList    as_buffer() const { return BufferList({header_buffer(), body_buffer()}); }
    private:

        const_buffer  header_buffer() const { return boost::asio::buffer((const void*)&m_Header, sizeof(m_Header)); }
        const_buffer  body_buffer()   const { return boost::asio::buffer((const void*)&m_Body[0], m_Body.size()); }
        Header      m_Header;
        std::string m_Body;
    };
}
