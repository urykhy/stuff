#pragma once

#include <cassert>
#include <map>
#include <memory>

#include "Router.hpp"

#include <container/Stream.hpp>
#include <nghttp2/nghttp2.h>
#include <string/String.hpp>
#include <threads/Asio.hpp>
#include <unsorted/Enum.hpp>
#include <unsorted/Log4cxx.hpp>

namespace asio_http::v2 {

    enum class Type : uint8_t
    {
        DATA          = 0,
        HEADERS       = 1,
        SETTINGS      = 4,
        GOAWAY        = 7,
        WINDOW_UPDATE = 8,
    };

    enum Setting : uint16_t
    {
        HEADER_TABLE_SIZE      = 0x1,
        MAX_CONCURRENT_STREAMS = 0x3,
        INITIAL_WINDOW_SIZE    = 0x4,
        MAX_FRAME_SIZE         = 0x5,
        MAX_HEADER_LIST_SIZE   = 0x6,
    };

    constexpr size_t MIN_FRAME_SIZE            = 4096;
    constexpr size_t MAX_STREAM_EXCLUSIVE      = 131072;
    constexpr size_t DEFAULT_HEADER_TABLE_SIZE = 4096;
    constexpr size_t DEFAULT_MAX_FRAME_SIZE    = 16384;
    constexpr size_t DEFAULT_WINDOW_SIZE       = 65535;

    enum Flags : uint8_t
    {
        ACK_SETTINGS = 0x1,
        END_STREAM   = 0x1,
        END_HEADERS  = 0x4,
        PADDED       = 0x8,
        PRIORITY     = 0x20,
    };
    _DECLARE_ENUM_OPS(Flags)

    struct Header
    {
        uint32_t size : 24 = {};
        Type     type      = {};
        Flags    flags     = {};
        uint32_t stream    = {};

        void to_host()
        {
            size   = be32toh(size << 8);
            stream = be32toh(stream);
        }
        void to_net()
        {
            size   = htobe32(size) >> 8;
            stream = htobe32(stream);
        }
    } __attribute__((packed));

    struct SettingVal
    {
        uint16_t key   = {};
        uint32_t value = {};

        void to_host()
        {
            key   = be16toh(key);
            value = be32toh(value);
        }
    } __attribute__((packed));

    class Session
    {
        using InflaterT = std::unique_ptr<nghttp2_hd_inflater, void (*)(nghttp2_hd_inflater*)>;
        using DeflaterT = std::unique_ptr<nghttp2_hd_deflater, void (*)(nghttp2_hd_deflater*)>;

        beast::error_code   m_Ec;
        asio::io_service&   m_Service;
        beast::tcp_stream&  m_Stream;
        RouterPtr           m_Router;
        asio::yield_context m_Yield;
        InflaterT           m_Inflate;
        DeflaterT           m_Deflate;
        Header              m_Header;
        std::string         m_Buffer;

        // frame control

        struct Stream
        {
            uint32_t    m_Budget = DEFAULT_WINDOW_SIZE;
            Request     m_Request;
            std::string m_Body;
            bool        m_Ready = false;
        };

        uint32_t                   m_Budget = DEFAULT_WINDOW_SIZE;
        std::map<uint32_t, Stream> m_Streams;

        void frames(uint32_t aStream, std::string_view aData, bool aLast)
        {
            while (!aData.empty()) {
                const size_t sLen = std::min(aData.size(), DEFAULT_MAX_FRAME_SIZE);

                Header sHeader;
                sHeader.size = sLen;
                sHeader.type = Type::DATA;
                if (sLen == aData.size() and aLast)
                    sHeader.flags = Flags::END_STREAM;
                sHeader.stream = aStream;
                send(sHeader);
                DEBUG("sent data header");

                send(aData.substr(0, sLen));
                DEBUG("sent data body " << sLen << " bytes");
                aData.remove_prefix(sLen);
            }
        }

        bool flush_step()
        {
            bool sMore = false;

            // iterate and flush as much as we can
            for (auto x = m_Streams.begin(); x != m_Streams.end();) {
                auto& sStream = x->first;
                auto& sData   = x->second;
                auto& sBody   = sData.m_Body;
                if (!sData.m_Ready) { // request not processed yet
                    x++;
                    continue;
                }
                if (sBody.size() == 0) { // no more data to send
                    x = m_Streams.erase(x);
                    continue;
                }

                auto sGetLen = [&]() {
                    return std::min({(size_t)m_Budget,
                                     (size_t)sData.m_Budget,
                                     sBody.size(),
                                     MAX_STREAM_EXCLUSIVE});
                };
                size_t sLen = sGetLen();

                auto sNoBudget = [&]() {
                    return sLen < MIN_FRAME_SIZE and sBody.size() > sLen;
                };
                if (sNoBudget()) {
                    DEBUG("low budget " << sLen << " for stream " << sStream << " (" << sBody.size() << " bytes pending)");
                    x++;
                    continue;
                }

                const bool sLast = sLen == sBody.size();
                frames(sStream, std::string_view(sBody.data(), sLen), sLast);
                m_Budget -= sLen;

                if (sLast) {
                    DEBUG("stream " << sStream << " done");
                    x = m_Streams.erase(x);
                } else {
                    sData.m_Budget -= sLen;
                    sBody.erase(0, sLen);
                    DEBUG("stream " << sStream << " have " << sBody.size() << " bytes pending");
                    x++;
                    sLen = sGetLen();
                    sMore |= !sNoBudget();
                }
            }

            return sMore;
        }

        void flush()
        {
            while (true) {
                if (m_Budget < MIN_FRAME_SIZE) {
                    DEBUG("connection budget low: " << m_Budget);
                    return;
                }
                if (!flush_step())
                    return;
            }
        }

        // Recv

        void recv_header()
        {
            boost::asio::async_read(m_Stream, boost::asio::buffer(&m_Header, sizeof(m_Header)), m_Yield[m_Ec]);
            if (m_Ec)
                throw m_Ec;
            m_Header.to_host();
        }

        void recv_body(size_t aSize)
        {
            m_Buffer.resize(aSize);
            boost::asio::async_read(m_Stream, boost::asio::buffer(m_Buffer.data(), aSize), m_Yield[m_Ec]);
            if (m_Ec)
                throw m_Ec;
        }

        // Send

        void send(Header aData) // copy here
        {
            aData.to_net();
            boost::asio::async_write(m_Stream, boost::asio::const_buffer(&aData, sizeof(aData)), m_Yield[m_Ec]);
            if (m_Ec)
                throw m_Ec;
        }

        void send(std::string_view aStr)
        {
            boost::asio::async_write(m_Stream, boost::asio::const_buffer(aStr.data(), aStr.size()), m_Yield[m_Ec]);
            if (m_Ec)
                throw m_Ec;
        }

        void send(const Response& aResponse)
        {
            std::list<std::string>  sShadow;
            std::vector<nghttp2_nv> sPairs;

            auto sAssign = [&sPairs](boost::beast::string_view sName, boost::beast::string_view sValue) {
                nghttp2_nv sNV;
                sNV.name     = (uint8_t*)sName.data();
                sNV.namelen  = sName.size();
                sNV.value    = (uint8_t*)sValue.data();
                sNV.valuelen = sValue.size();
                sNV.flags    = NGHTTP2_NV_FLAG_NONE;
                sPairs.push_back(sNV);
            };

            sShadow.push_back(std::to_string(aResponse.result_int()));
            sAssign(":status", sShadow.back());
            for (auto& x : aResponse) {
                sShadow.push_back(x.name_string().to_string());
                String::tolower(sShadow.back());
                sAssign(sShadow.back(), x.value());
            }

            size_t sLength = nghttp2_hd_deflate_bound(m_Deflate.get(), sPairs.data(), sPairs.size());
            m_Buffer.resize(sLength);
            ssize_t sUsed = nghttp2_hd_deflate_hd(m_Deflate.get(), (uint8_t*)m_Buffer.data(), sLength, sPairs.data(), sPairs.size());

            if (sUsed < 0)
                throw std::runtime_error("fail to deflate headers");

            m_Buffer.resize(sUsed);
            DEBUG("response headers compressed to " << sUsed << " bytes");

            Header sHeader;
            sHeader.size   = sUsed;
            sHeader.type   = Type::HEADERS;
            sHeader.flags  = Flags::END_HEADERS;
            sHeader.stream = m_Header.stream;

            if (aResponse.body().empty())
                sHeader.flags |= Flags::END_STREAM;

            send(sHeader);
            DEBUG("sent header");

            send(m_Buffer);
            DEBUG("sent header body");

            auto sIt = m_Streams.find(m_Header.stream);
            assert(sIt != m_Streams.end());
            if (aResponse.body().empty()) {
                m_Streams.erase(sIt);
            } else {
                sIt->second.m_Body  = std::move(aResponse.body());
                sIt->second.m_Ready = true;
            }
        }

        // Protocol handling

        void call(Request& aRequest)
        {
            Response sResponse;
            m_Router->call(m_Service, aRequest, sResponse, m_Yield[m_Ec]);
            if (m_Ec)
                throw m_Ec;
            sResponse.prepare_payload();
            if (0 == sResponse.count(http::field::server))
                sResponse.set(http::field::server, "Beast/cxx");
            send(sResponse);
        }

        void process_data()
        {
            DEBUG("got " << m_Buffer.size() << " bytes of payload");
            Request& sRequest = m_Streams[m_Header.stream].m_Request;
            sRequest.body().append(std::move(m_Buffer));

            if (m_Header.flags & Flags::END_STREAM) {
                DEBUG("got complete request")
                call(sRequest);
            }
        }

        void collect(nghttp2_nv sHeader, Request& aRequest)
        {
            std::string sName((const char*)sHeader.name, sHeader.namelen);
            std::string sValue((const char*)sHeader.value, sHeader.valuelen);
            DEBUG(sName << ": " << sValue);
            if (sName == ":method")
                aRequest.method(http::string_to_verb(sValue));
            else if (sName == ":path")
                aRequest.target(sValue);
            else if (sName == ":authority")
                aRequest.set(http::field::host, sValue);
            else if (sName.size() > 1 and sName[0] == ':')
                ;
            else
                aRequest.set(sName, sValue);
        }

        void process_headers()
        {
            assert(m_Header.stream != 0);
            auto& sRequest = m_Streams[m_Header.stream].m_Request;

            Container::imemstream sData(m_Buffer);

            uint8_t  sPadLength = 0;
            uint32_t sStream    = 0;
            uint8_t  sPrio      = 0;
            if (m_Header.flags & Flags::PADDED)
                sData.read(sPadLength);
            if (m_Header.flags & Flags::PRIORITY) {
                sData.read(sStream);
                sData.read(sPrio);
            }
            std::string_view sRest = sData.rest();
            sRest.remove_suffix(sPadLength);
            DEBUG("got headers block of " << sRest.size() << " bytes");

            while (!sRest.empty()) {
                nghttp2_nv sHeader;
                int        sFlags = 0;

                int sUsed = nghttp2_hd_inflate_hd2(m_Inflate.get(),
                                                   &sHeader,
                                                   &sFlags,
                                                   (const uint8_t*)sRest.data(),
                                                   sRest.size(),
                                                   sHeader.flags & Flags::END_HEADERS);

                if (sUsed < 0)
                    throw std::runtime_error("fail to inflate headers");
                sRest.remove_prefix(sUsed);
                if (sFlags & NGHTTP2_HD_INFLATE_EMIT)
                    collect(sHeader, sRequest);
                if (sFlags & NGHTTP2_HD_INFLATE_FINAL)
                    nghttp2_hd_inflate_end_headers(m_Inflate.get());
            }

            if (m_Header.flags & Flags::END_STREAM)
                call(sRequest);
        }

        void process_settings()
        {
            if (m_Header.flags != 0) // filter out ACK_SETTINGS
                return;

            Container::imemstream sData(m_Buffer);
            while (!sData.eof()) {
                SettingVal sVal;
                sData.read(sVal);
                sVal.to_host();
                DEBUG(sVal.key << ": " << sVal.value);
            }
            Header sAck;
            sAck.type  = Type::SETTINGS;
            sAck.flags = Flags::ACK_SETTINGS;
            send(sAck);
            DEBUG("ack settings");
        }

        void process_window_update()
        {
            assert(m_Buffer.size() == 4);
            Container::imemstream sData(m_Buffer);

            uint32_t sInc = 0;
            sData.read(sInc);
            sInc = be32toh(sInc);
            sInc &= 0x7FFFFFFFFFFFFFFF; // clear R bit

            if (m_Header.stream == 0) {
                m_Budget += sInc;
                DEBUG("connection window increment " << sInc << ", now " << m_Budget);
            } else {
                auto& sBudget = m_Streams[m_Header.stream].m_Budget;
                sBudget += sInc;
                DEBUG("stream window increment " << sInc << ", now " << sBudget);
            }
        }

    public:
        Session(asio::io_service& aService, beast::tcp_stream& aStream, RouterPtr aRouter, asio::yield_context yield)
        : m_Service(aService)
        , m_Stream(aStream)
        , m_Router(aRouter)
        , m_Yield(yield)
        , m_Inflate([]() {
            nghttp2_hd_inflater* sTmp = nullptr;
            if (nghttp2_hd_inflate_new(&sTmp))
                throw std::runtime_error("fail to initialize nghttp/inflater");
            return InflaterT(sTmp, nghttp2_hd_inflate_del);
        }())
        , m_Deflate([]() {
            nghttp2_hd_deflater* sTmp = nullptr;
            if (nghttp2_hd_deflate_new(&sTmp, DEFAULT_HEADER_TABLE_SIZE))
                throw std::runtime_error("fail to initialize nghttp/deflater");
            return DeflaterT(sTmp, nghttp2_hd_deflate_del);
        }())
        {}

        void hello()
        {
            const std::string_view sExpected("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");

            DEBUG("wait for preface");
            recv_body(sExpected.size());

            if (m_Buffer != sExpected)
                throw std::runtime_error("not a http/2 peer");

            Header sAck;
            sAck.type = Type::SETTINGS;
            send(sAck);
        }

        bool process_frame()
        {
            recv_header();
            DEBUG("got header, size: " << m_Header.size << ", type: " << (int)m_Header.type << ", flags: " << (int)m_Header.flags << ", stream: " << m_Header.stream);

            if (m_Header.size > 0)
                recv_body(m_Header.size);
            else
                m_Buffer.clear();

            switch (m_Header.type) {
            case Type::DATA: process_data(); break;
            case Type::HEADERS: process_headers(); break;
            case Type::SETTINGS: process_settings(); break;
            case Type::GOAWAY: return false; break;
            case Type::WINDOW_UPDATE: process_window_update(); break;
            default: break;
            }

            flush();

            return true;
        }
    };

    inline void session2(asio::io_service& aService, beast::tcp_stream& aStream, RouterPtr aRouter, asio::yield_context yield)
    {
        try {
            Session sSession(aService, aStream, aRouter, yield);
            sSession.hello();
            DEBUG("http/2 negotiated");

            while (true) {
                aStream.expires_after(std::chrono::seconds(30));
                if (!sSession.process_frame())
                    break; // GO AWAY
            }
        } catch (const beast::error_code e) {
            ERROR("beast error: " << e);
        } catch (const std::exception& e) {
            ERROR("exception: " << e.what());
        }

        beast::error_code ec;
        aStream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    inline void server2(asio::io_service& aService, std::shared_ptr<tcp::acceptor> aAcceptor, std::shared_ptr<tcp::socket> aSocket, RouterPtr aRouter)
    {
        aAcceptor->async_accept(*aSocket, [aService = std::ref(aService), aAcceptor, aSocket, aRouter](beast::error_code ec) {
            if (!ec) {
                aSocket->set_option(tcp::no_delay(true));
                boost::asio::spawn(aAcceptor->get_executor(), [aService, sStream = beast::tcp_stream(std::move(*aSocket)), aRouter](boost::asio::yield_context yield) mutable {
                    session2(aService, sStream, aRouter, yield);
                });
            }
            server2(aService, aAcceptor, aSocket, aRouter);
        });
    }

    inline void startServer(Threads::Asio& aContext, uint16_t aPort, RouterPtr aRouter)
    {
        auto const sAddress  = asio::ip::make_address("0.0.0.0");
        auto       sAcceptor = std::make_shared<tcp::acceptor>(aContext.service(), tcp::endpoint(sAddress, aPort));
        auto       sSocket   = std::make_shared<tcp::socket>(aContext.service());
        server2(aContext.service(), sAcceptor, sSocket, aRouter);
    }
} // namespace asio_http::v2