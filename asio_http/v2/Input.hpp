#pragma once

#include "HPack.hpp"
#include "Types.hpp"

namespace asio_http::v2 {

    struct Frame
    {
        Header      header;
        std::string body;
    };

    struct InputFace
    {
        virtual void process_settings(const Frame& aFrame)                 = 0;
        virtual void process_window_update(const Frame& aFrame)            = 0;
        virtual void emit_window_update(uint32_t aStreamId, uint32_t aInc) = 0;
        virtual void process_request(uint32_t aStreamId, Request&& aRequest){};
        virtual void process_response(uint32_t aStreamId, Response&& aResponse){};
        virtual ~InputFace(){};
    };

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
        bool read(void* aAddr, size_t aSize)
        {
            if (readAvail() < aSize)
                return false;
            memmove(aAddr, &m_Data[m_readAt], aSize);
            m_readAt += aSize;
            return true;
        }
        bool append(std::string& aStr, size_t aSize)
        {
            size_t sSize = std::min(aSize, readAvail());
            aStr.append(&m_Data[m_readAt], sSize);
            m_readAt += sSize;
            return aSize == sSize;
        }
    };

    class Input
    {
        beast::tcp_stream& m_Stream;
        CoroState*         m_Coro = nullptr;
        InputFace*         m_Face;
        Inflate            m_Inflate;
        const bool         m_Server;

        struct Info
        {
            Request     request;
            Response    response;
            std::string body;
            uint32_t    budget  = DEFAULT_WINDOW_SIZE;
            bool        no_body = false;
        };
        std::map<uint32_t, Info> m_Info;

        uint32_t m_Budget = DEFAULT_WINDOW_SIZE; // connection budget

        bool     m_State = false; // wait for header. true = read body
        InputBuf m_Buffer;

        void emit_window_update(uint32_t aStreamId, uint32_t& aCurrent)
        {
            TRACE("request window update for " << aStreamId << ", was " << aCurrent);
            m_Face->emit_window_update(aStreamId, DEFAULT_WINDOW_SIZE);
            aCurrent += DEFAULT_WINDOW_SIZE;
        }

        void data_cb(uint32_t aStreamId, Info& aInfo)
        {
            if (m_Server) {
                aInfo.request.body() = std::move(aInfo.body);
                m_Face->process_request(aStreamId, std::move(aInfo.request));
            } else {
                aInfo.response.body() = std::move(aInfo.body);
                m_Face->process_response(aStreamId, std::move(aInfo.response));
            }
        }

        void process_data(const Frame& aFrame)
        {
            const uint32_t sStreamId = aFrame.header.stream;
            if (sStreamId == 0)
                throw std::runtime_error("stream id = 0");
            auto sIt = m_Info.find(sStreamId);
            if (sIt == m_Info.end())
                throw std::runtime_error("stream not found");
            auto& sInfo = sIt->second;

            // account budget
            const bool sMore = aFrame.header.flags ^ Flags::END_STREAM;
            assert(m_Budget >= aFrame.body.size());
            assert(sInfo.budget >= aFrame.body.size());
            sInfo.body += aFrame.body;
            sInfo.budget -= aFrame.body.size();
            m_Budget -= aFrame.body.size();
            if (m_Budget < DEFAULT_WINDOW_SIZE) {
                emit_window_update(0, m_Budget);
            }
            if (sInfo.budget < DEFAULT_WINDOW_SIZE and sMore) {
                emit_window_update(sStreamId, sInfo.budget);
            }

            // request(response) collected
            if (aFrame.header.flags & Flags::END_STREAM) {
                data_cb(sStreamId, sInfo);
                m_Info.erase(sIt);
            }
        }

        // FIXME: ensure stream already exists if CONTINUATION
        //        ensure new stream if HEADERS
        void process_headers(const Frame& aFrame)
        {
            const uint32_t sStreamId = aFrame.header.stream;
            if (sStreamId == 0)
                throw std::runtime_error("zero stream id");
            auto& sInfo = m_Info[sStreamId];

            if (m_Server)
                m_Inflate(aFrame.header, aFrame.body, sInfo.request);
            else
                m_Inflate(aFrame.header, aFrame.body, sInfo.response);

            if (aFrame.header.flags & Flags::END_STREAM)
                sInfo.no_body = true;
            if (aFrame.header.flags & Flags::END_HEADERS and sInfo.no_body) {
                data_cb(sStreamId, sInfo);
                m_Info.erase(sStreamId);
            }
        }

    public:
        Input(beast::tcp_stream& aStream, InputFace* aFace, bool aServer)
        : m_Stream(aStream)
        , m_Face(aFace)
        , m_Server(aServer)
        {
        }

        void assign(CoroState* aCoro)
        {
            m_Coro = aCoro;
        }

        Frame recv()
        {
            Frame sTmp;
            while (true) {
                if (!m_State) {
                    if (m_Buffer.read(&sTmp.header, sizeof(Header))) {
                        m_State = true; // wait for body
                        sTmp.header.to_host();
                        TRACE("frame header; size: "
                              << sTmp.header.size << "; type: "
                              << sTmp.header.type << "; flags: "
                              << sTmp.header.flags << "; stream: "
                              << sTmp.header.stream);
                        sTmp.body.reserve(sTmp.header.size);
                    }
                }
                if (m_State) {
                    size_t sSize = sTmp.header.size - sTmp.body.size();
                    if (m_Buffer.append(sTmp.body, sSize)) {
                        m_State = false; // wait for next header
                        return sTmp;
                    }
                }
                size_t sNew = m_Stream.async_read_some(m_Buffer.buffer(), m_Coro->yield[m_Coro->ec]);
                if (m_Coro->ec)
                    throw m_Coro->ec;
                TRACE("recv " << sNew << " bytes from client");
                m_Buffer.push(sNew);
            }
        }

        void process_frame()
        {
            m_Stream.expires_after(std::chrono::seconds(30)); // FIXME configurable timeout
            auto sFrame = recv();

            //auto sHolder = g_Profiler.start("input", "process");

            switch (sFrame.header.type) {
            case Type::DATA: process_data(sFrame); break;
            case Type::HEADERS: process_headers(sFrame); break;
            case Type::SETTINGS: m_Face->process_settings(sFrame); break;
            case Type::WINDOW_UPDATE: m_Face->process_window_update(sFrame); break;
            case Type::CONTINUATION: process_headers(sFrame); break;
            default: break;
            }
        }
    };
} // namespace asio_http::v2