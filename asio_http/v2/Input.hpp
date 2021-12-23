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

        void process_frame()
        {
            m_Stream.expires_after(std::chrono::seconds(30)); // FIXME configurable timeout
            auto sFrame = recv();

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