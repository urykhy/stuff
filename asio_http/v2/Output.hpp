#pragma once

#include "Types.hpp"

#include <container/Stream.hpp>

namespace asio_http::v2 {

    class Output
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

        void send(uint32_t aStreamId, std::string_view aData, bool aLast)
        {
            while (!aData.empty()) {
                const size_t sLen = std::min(aData.size(), DEFAULT_MAX_FRAME_SIZE);

                Header sHeader;
                sHeader.type = Type::DATA;
                if (sLen == aData.size() and aLast)
                    sHeader.flags = Flags::END_STREAM;
                sHeader.stream = aStreamId;
                send(sHeader, aData.substr(0, sLen), m_Coro);
                aData.remove_prefix(sLen);
            }
        }

    public:
        Output(beast::tcp_stream& aStream)
        : m_Stream(aStream)
        {
        }

        void assign(CoroState* aCoro)
        {
            m_Coro = aCoro;
        }

        void enqueue(uint32_t aStreamId, std::string&& aBody)
        {
            auto& sInfo = m_Info[aStreamId];
            sInfo.body  = std::move(aBody);
        }

        void flush()
        {
            for (auto x = m_Info.begin(); x != m_Info.end();) {
                auto& sStream = x->first;
                auto& sData   = x->second;
                auto& sBody   = sData.body;

                if (sBody.size() == 0) { // no more data to send
                    x = m_Info.erase(x);
                    continue;
                }

                const size_t sLen = std::min({(size_t)m_Budget,
                                              (size_t)sData.budget,
                                              sBody.size(),
                                              MAX_STREAM_EXCLUSIVE});

                const bool sNoBudget = sLen < MIN_FRAME_SIZE and sBody.size() > sLen;

                if (sNoBudget) {
                    TRACE("low budget " << sLen << " for stream " << sStream << " (" << sBody.size() << " bytes pending)");
                    x++;
                    continue;
                }

                const bool sLast = sLen == sBody.size();
                send(sStream, std::string_view(sBody.data(), sLen), sLast);
                m_Budget -= sLen;
                TRACE("sent " << sLen << " bytes for stream " << sStream);

                if (sLast) {
                    TRACE("stream " << sStream << " done");
                    x = m_Info.erase(x);
                } else {
                    sData.budget -= sLen;
                    sBody.erase(0, sLen);
                    TRACE("stream " << sStream << " have " << sBody.size() << " bytes pending");
                    x++;
                }
            }
        }

        void window_update(const Header& aHeader, const std::string& aData)
        {
            if (aData.size() != 4)
                throw std::runtime_error("invalid window update");
            Container::imemstream sData(aData);

            uint32_t sInc = 0;
            sData.read(sInc);
            sInc = be32toh(sInc);
            sInc &= 0x7FFFFFFFFFFFFFFF; // clear R bit

            if (aHeader.stream == 0) {
                m_Budget += sInc;
                TRACE("connection window increment " << sInc << ", now " << m_Budget);
            } else {
                auto& sBudget = m_Info[aHeader.stream].budget;
                sBudget += sInc;
                TRACE("stream window increment " << sInc << ", now " << sBudget);
            }
        }

        bool idle() const
        {
            return m_Info.empty();
        }

        void send(Header aHeader, std::string_view aStr, CoroState* aCoro) // copy header here
        {
            aHeader.size = aStr.size();
            aHeader.to_net();

            std::array<asio::const_buffer, 2> sBuffer = {
                asio::const_buffer(&aHeader, sizeof(aHeader)),
                asio::const_buffer(aStr.data(), aStr.size())};

            asio::async_write(m_Stream, sBuffer, aCoro->yield[aCoro->ec]);
            if (aCoro->ec)
                throw aCoro->ec;
        }
    };
} // namespace asio_http::v2