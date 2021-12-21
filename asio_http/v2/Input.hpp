#pragma once

#include "Types.hpp"

namespace asio_http::v2 {

    // input queue
    // sent window updates as required
    class Input
    {
        beast::tcp_stream& m_Stream;
        CoroState*         m_Coro = nullptr;

        struct Info
        {
            std::string body;
            uint32_t    budget = DEFAULT_WINDOW_SIZE;
        };
        std::map<uint32_t, Info> m_Info;

        uint32_t m_Budget = DEFAULT_WINDOW_SIZE; // connection budget

        void send(Header aHeader, uint32_t aInc) // copy header here
        {
            aHeader.size = sizeof(aInc);
            aHeader.to_net();

            std::array<asio::const_buffer, 2> sBuffer = {
                asio::const_buffer(&aHeader, sizeof(aHeader)),
                asio::const_buffer(&aInc, sizeof(aInc))};

            asio::async_write(m_Stream, sBuffer, m_Coro->yield[m_Coro->ec]);
            if (m_Coro->ec)
                throw m_Coro->ec;
        }

        void window_update(uint32_t aStreamId, uint32_t& aCurrent)
        {
            Header sHeader;
            sHeader.type   = Type::WINDOW_UPDATE;
            sHeader.stream = aStreamId;
            send(sHeader, htobe32(DEFAULT_WINDOW_SIZE));
            TRACE("sent window update for " << aStreamId << ", was " << aCurrent);
            aCurrent += DEFAULT_WINDOW_SIZE;
        }

    public:
        Input(beast::tcp_stream& aStream)
        : m_Stream(aStream)
        {
        }

        void assign(CoroState* aCoro)
        {
            m_Coro = aCoro;
        }

        void append(uint32_t aStreamId, const std::string& aData, bool aMore)
        {
            auto& sInfo = m_Info[aStreamId];

            assert(m_Budget >= aData.size());
            assert(sInfo.budget >= aData.size());

            sInfo.body += aData;
            sInfo.budget -= aData.size();
            m_Budget -= aData.size();

            if (m_Budget < DEFAULT_WINDOW_SIZE) {
                window_update(0, m_Budget);
            }
            if (sInfo.budget < DEFAULT_WINDOW_SIZE and aMore) {
                window_update(aStreamId, sInfo.budget);
            }
        }

        std::string extract(uint32_t aStreamId)
        {
            auto sIt = m_Info.find(aStreamId);
            if (sIt == m_Info.end())
                throw std::runtime_error("Input: stream data not found");

            std::string sTmp = std::move(sIt->second.body);
            m_Info.erase(sIt);
            return sTmp;
        }

        struct Frame
        {
            Header      header;
            std::string body;
        };
        Frame recv()
        {
            Frame sTmp;

            asio::async_read(m_Stream, asio::buffer(&sTmp.header, sizeof(Header)), m_Coro->yield[m_Coro->ec]);
            sTmp.header.to_host();

            TRACE("frame header; size: "
                  << sTmp.header.size << "; type: "
                  << sTmp.header.type << "; flags: "
                  << sTmp.header.flags << "; stream: "
                  << sTmp.header.stream);
            if (m_Coro->ec)
                throw m_Coro->ec;
            if (sTmp.header.size > 0) {
                sTmp.body.resize(sTmp.header.size);
                asio::async_read(m_Stream, asio::buffer(sTmp.body.data(), sTmp.body.size()), m_Coro->yield[m_Coro->ec]);
                if (m_Coro->ec)
                    throw m_Coro->ec;
            }
            return sTmp;
        }
    };
}