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
        CONTINUATION  = 9,
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

    class Session : public std::enable_shared_from_this<Session>
    {
        using InflaterT = std::unique_ptr<nghttp2_hd_inflater, void (*)(nghttp2_hd_inflater*)>;
        using DeflaterT = std::unique_ptr<nghttp2_hd_deflater, void (*)(nghttp2_hd_deflater*)>;

        asio::io_service&        m_Service;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;
        RouterPtr                m_Router;
        InflaterT                m_Inflate;
        DeflaterT                m_Deflate;
        Header                   m_Header;
        std::string              m_Buffer;

        struct ResponseItem
        {
            uint32_t stream_id;
            Response response;
        };
        std::list<ResponseItem> m_WriteQueue;

        struct CoroState
        {
            beast::error_code   ec;
            asio::yield_context yield;
        };
        std::unique_ptr<CoroState> m_ReadCoro;
        std::unique_ptr<CoroState> m_WriteCoro;
        asio::steady_timer         m_Timer;

        // frame control

        struct Stream
        {
            uint32_t    m_Budget = DEFAULT_WINDOW_SIZE;
            Request     m_Request;
            std::string m_Body;
            bool        m_Ready = false;
            bool        m_NoBody = false;
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
                send(sHeader, m_WriteCoro);
                DEBUG("sent data header");

                send(aData.substr(0, sLen), m_WriteCoro);
                DEBUG("sent data body " << sLen << " bytes");
                aData.remove_prefix(sLen);
            }
        }

        bool flush_step()
        {
            bool sNoMore = true;
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

                const size_t sLen      = std::min({(size_t)m_Budget,
                                              (size_t)sData.m_Budget,
                                              sBody.size(),
                                              MAX_STREAM_EXCLUSIVE});
                const bool   sNoBudget = sLen < MIN_FRAME_SIZE and sBody.size() > sLen;

                if (sNoBudget) {
                    DEBUG("low budget " << sLen << " for stream " << sStream << " (" << sBody.size() << " bytes pending)");
                    x++;
                    sNoMore = false;
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
                    sNoMore = false; // have more data to write
                }
            }
            return sNoMore;
        }

        bool flush() // return true if no more data to write out
        {
            DEBUG("flush responses...");

            while (!m_WriteQueue.empty()) {
                send(std::move(m_WriteQueue.front()));
                m_WriteQueue.pop_front();
            }

            return flush_step();
        }

        // Recv

        void
        recv_header()
        {
            asio::async_read(m_Stream, asio::buffer(&m_Header, sizeof(m_Header)), m_ReadCoro->yield[m_ReadCoro->ec]);
            m_Header.to_host();
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;
        }

        void recv_body(size_t aSize)
        {
            m_Buffer.resize(aSize);
            asio::async_read(m_Stream, asio::buffer(m_Buffer.data(), aSize), m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;
        }

        // Send

        void send(Header aData, std::unique_ptr<CoroState>& aState) // copy here
        {
            aData.to_net();
            asio::async_write(m_Stream, asio::const_buffer(&aData, sizeof(aData)), aState->yield[aState->ec]);
            if (aState->ec)
                throw aState->ec;
        }

        void send(std::string_view aStr, std::unique_ptr<CoroState>& aState)
        {
            asio::async_write(m_Stream, asio::const_buffer(aStr.data(), aStr.size()), aState->yield[aState->ec]);
            if (aState->ec)
                throw aState->ec;
        }

        void send(ResponseItem&& aResponse)
        {
            std::list<std::string>  sShadow;
            std::vector<nghttp2_nv> sPairs;

            auto& sResponse = aResponse.response;

            auto sAssign = [&sPairs](boost::beast::string_view sName, boost::beast::string_view sValue) {
                nghttp2_nv sNV;
                sNV.name     = (uint8_t*)sName.data();
                sNV.namelen  = sName.size();
                sNV.value    = (uint8_t*)sValue.data();
                sNV.valuelen = sValue.size();
                sNV.flags    = NGHTTP2_NV_FLAG_NONE;
                sPairs.push_back(sNV);
            };

            sShadow.push_back(std::to_string(sResponse.result_int()));
            sAssign(":status", sShadow.back());
            for (auto& x : sResponse) {
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
            sHeader.stream = aResponse.stream_id;

            if (sResponse.body().empty())
                sHeader.flags |= Flags::END_STREAM;

            send(sHeader, m_WriteCoro);
            DEBUG("sent header");

            send(m_Buffer, m_WriteCoro);
            DEBUG("sent header body");

            auto sIt = m_Streams.find(aResponse.stream_id);
            assert(sIt != m_Streams.end());
            if (sResponse.body().empty()) {
                m_Streams.erase(sIt);
            } else {
                sIt->second.m_Body  = std::move(sResponse.body());
                sIt->second.m_Ready = true;
            }
        }

        // Protocol handling

        void call(Request&& aRequest)
        {
            uint32_t sStreamId = m_Header.stream;
            // make router->call in any asio thread,
            // process result in proper strand

            DEBUG("spawn to perform call for stream " << sStreamId);
            asio::spawn(
                m_Service,
                [p = shared_from_this(), sStreamId, aRequest = std::move(aRequest)](asio::yield_context yield) mutable {
                    beast::error_code ec;
                    Response          sResponse;

                    p->m_Router->call(p->m_Service, aRequest, sResponse, yield[ec]);

                    sResponse.prepare_payload();
                    if (0 == sResponse.count(http::field::server))
                        sResponse.set(http::field::server, "Beast/cxx");

                    p->m_Strand.post([p, sStreamId, sResponse = std::move(sResponse)]() mutable {
                        p->call_cb(sStreamId, std::move(sResponse));
                    });
                });
        }

        void call_cb(uint32_t aStreamId, Response&& aResponse)
        {
            const bool sStart = m_WriteQueue.empty();
            DEBUG("queued response for stream " << aStreamId);
            m_WriteQueue.push_back(ResponseItem{aStreamId, std::move(aResponse)});
            if (sStart and !m_WriteCoro)
                spawn_write_coro();
        }

        void process_data()
        {
            DEBUG("got " << m_Buffer.size() << " bytes of payload");
            Request& sRequest = m_Streams[m_Header.stream].m_Request;
            sRequest.body().append(std::move(m_Buffer));

            if (m_Header.flags & Flags::END_STREAM) {
                DEBUG("got complete request")
                call(std::move(sRequest));
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
            // if continuation -> asert we already have seen headers frame
            auto& sStream  = m_Streams[m_Header.stream];
            auto& sRequest = sStream.m_Request;

            Container::imemstream sData(m_Buffer);

            uint8_t  sPadLength = 0;
            if (m_Header.flags & Flags::PADDED)
                sData.read(sPadLength);
            if (m_Header.flags & Flags::PRIORITY) {
                sData.skip(4 + 1);  // stream id + prio
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
                sStream.m_NoBody = true;

            if (m_Header.flags & Flags::END_HEADERS and sStream.m_NoBody)
            {
                DEBUG("got complete request")
                call(std::move(sRequest));
            }
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
            send(sAck, m_ReadCoro);
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

        void hello()
        {
            const std::string_view sExpected("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");

            DEBUG("wait for preface");
            recv_body(sExpected.size());

            if (m_Buffer != sExpected)
                throw std::runtime_error("not a http/2 peer");

            Header sAck;
            sAck.type = Type::SETTINGS;
            send(sAck, m_ReadCoro);
        }

        bool process_frame()
        {
            m_Stream.expires_after(std::chrono::seconds(30));
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
            case Type::CONTINUATION: process_headers(); break;
            default: break;
            }

            return true;
        }

    public:
        Session(asio::io_service& aService, beast::tcp_stream&& aStream, RouterPtr aRouter)
        : m_Service(aService)
        , m_Strand(m_Service)
        , m_Stream(std::move(aStream))
        , m_Router(aRouter)
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
        , m_Timer(m_Service)
        {}

        ~Session()
        {
            beast::error_code ec;
            m_Stream.socket().shutdown(tcp::socket::shutdown_send, ec);
        }

        // coro magic

        void spawn_read_coro()
        {
            asio::spawn(m_Strand,
                        [p = shared_from_this()](asio::yield_context yield) mutable {
                            p->read_coro(yield);
                        });
        }

    private:
        void read_coro(asio::yield_context yield)
        {
            try {
                m_ReadCoro = std::make_unique<CoroState>(CoroState{{}, yield});
                hello();
                DEBUG("http/2 negotiated");

                while (true) {
                    if (!process_frame())
                        break; // GO AWAY
                    if (m_WriteCoro and m_WriteCoro->ec)
                        throw m_WriteCoro->ec;
                }
            } catch (const beast::error_code e) {
                ERROR("beast error: " << e);
            } catch (const std::exception& e) {
                ERROR("exception: " << e.what());
            }
        }

        void spawn_write_coro()
        {
            asio::spawn(m_Strand, [p = shared_from_this()](asio::yield_context yield) mutable {
                try {
                    p->write_coro(yield);
                } catch (const beast::error_code e) {
                    ERROR("beast error: " << e);
                }
            });
        }

        void write_coro(asio::yield_context yield)
        {
            DEBUG("write out coro started");
            m_WriteCoro = std::make_unique<CoroState>(CoroState{{}, yield});

            bool sNoMore = false; // no more m_Streams data to write
            auto sEnd    = [this, &sNoMore]() {
                return m_ReadCoro->ec or (m_WriteQueue.empty() and sNoMore);
            };

            while (!sEnd()) {
                m_Timer.expires_from_now(std::chrono::milliseconds(1));
                m_Timer.async_wait(m_WriteCoro->yield[m_WriteCoro->ec]);
                sNoMore = flush();
            }
            DEBUG("write out coro finished");
            m_WriteCoro.reset();
        }
    };

    inline void server2(asio::io_service& aService, std::shared_ptr<tcp::acceptor> aAcceptor, std::shared_ptr<tcp::socket> aSocket, RouterPtr aRouter)
    {
        aAcceptor->async_accept(*aSocket, [aService = std::ref(aService), aAcceptor, aSocket, aRouter](beast::error_code ec) {
            if (!ec) {
                aSocket->set_option(tcp::no_delay(true));
                auto sSession = std::make_shared<Session>(
                    aService,
                    beast::tcp_stream(std::move(*aSocket)),
                    aRouter);
                sSession->spawn_read_coro();
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