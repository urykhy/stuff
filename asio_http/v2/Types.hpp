#pragma once

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <type_traits>

#include <container/Stream.hpp>
#include <nghttp2/nghttp2.h>
#include <parser/Atoi.hpp>
#include <unsorted/Enum.hpp>
#include <unsorted/Raii.hpp>

namespace asio_http::v2 {

    inline log4cxx::LoggerPtr sLogger = Logger::Get("http");

    enum class Type : uint8_t
    {
        DATA          = 0,
        HEADERS       = 1,
        SETTINGS      = 4,
        GOAWAY        = 7,
        WINDOW_UPDATE = 8,
        CONTINUATION  = 9,
    };
    static const std::map<Type, std::string> sTypeNames = {
        {Type::DATA, "data"},
        {Type::HEADERS, "headers"},
        {Type::SETTINGS, "settings"},
        {Type::GOAWAY, "goaway"},
        {Type::WINDOW_UPDATE, "window"},
        {Type::CONTINUATION, "continuation"}};
    _DECLARE_ENUM_TO_STRING(Type, sTypeNames)

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
    static const std::map<Flags, std::string> sFlagsNames = {
        {END_STREAM, "end_stream"},
        {END_HEADERS, "end_headers"},
        {PADDED, "padded"},
        {PRIORITY, "priority"}
    };
    _DECLARE_SET_TO_STRING(Flags, sFlagsNames)
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

    struct CoroState
    {
        beast::error_code   ec;
        asio::yield_context yield;
    };

    class HPack
    {
        using InflaterT = std::unique_ptr<nghttp2_hd_inflater, void (*)(nghttp2_hd_inflater*)>;
        using DeflaterT = std::unique_ptr<nghttp2_hd_deflater, void (*)(nghttp2_hd_deflater*)>;

        InflaterT m_Inflate;
        DeflaterT m_Deflate;

        void collect(nghttp2_nv sHeader, Request& aRequest)
        {
            std::string sName((const char*)sHeader.name, sHeader.namelen);
            std::string sValue((const char*)sHeader.value, sHeader.valuelen);
            TRACE(sName << ": " << sValue);
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

        void collect(nghttp2_nv sHeader, Response& aResponse)
        {
            std::string sName((const char*)sHeader.name, sHeader.namelen);
            std::string sValue((const char*)sHeader.value, sHeader.valuelen);
            TRACE(sName << ": " << sValue);
            if (sName == ":status")
                aResponse.result(Parser::Atoi<unsigned>(sValue));
            else if (sName.size() > 1 and sName[0] == ':')
                ;
            else
                aResponse.set(sName, sValue);
        }

        std::string deflate(const std::vector<nghttp2_nv>& aPairs)
        {
            std::string sTmp;
            size_t      sLength = nghttp2_hd_deflate_bound(m_Deflate.get(), aPairs.data(), aPairs.size());
            sTmp.resize(sLength);
            ssize_t sUsed = nghttp2_hd_deflate_hd(m_Deflate.get(),
                                                  (uint8_t*)sTmp.data(),
                                                  sLength,
                                                  aPairs.data(),
                                                  aPairs.size());
            if (sUsed < 0)
                throw std::runtime_error("fail to deflate headers");
            sTmp.resize(sUsed);
            return sTmp;
        }

        struct Pairs
        {
            std::list<std::string>  m_Shadow;
            std::vector<nghttp2_nv> m_Pairs;

            void assign_ref(boost::beast::string_view sName, boost::beast::string_view sValue)
            {
                nghttp2_nv sNV;
                sNV.name     = (uint8_t*)sName.data();
                sNV.namelen  = sName.size();
                sNV.value    = (uint8_t*)sValue.data();
                sNV.valuelen = sValue.size();
                sNV.flags    = NGHTTP2_NV_FLAG_NONE;
                m_Pairs.push_back(sNV);
            }

            void assign_lower(boost::beast::string_view sName, boost::beast::string_view sValue)
            {
                m_Shadow.push_back(sName.to_string());
                String::tolower(m_Shadow.back());
                assign_ref(m_Shadow.back(), sValue);
            }
        };

    public:
        HPack()
        : m_Inflate([]() {
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

        std::string deflate(const ClientRequest& aRequest)
        {
            Pairs sPairs;

            sPairs.assign_ref(":method", http::to_string(aRequest.method));
            const auto sParsed = Parser::url(aRequest.url);
            sPairs.assign_ref(":path", sParsed.path);
            sPairs.assign_ref(":scheme", "http");
            sPairs.assign_ref(":authority", sParsed.host); // FIXME: add port

            for (auto& x : aRequest.headers)
                sPairs.assign_lower(x.first, x.second);

            return deflate(sPairs.m_Pairs);
        }

        std::string deflate(const Response& aResponse)
        {
            Pairs sPairs;

            const std::string sStatus = std::to_string(aResponse.result_int());
            sPairs.assign_ref(":status", sStatus);

            for (auto& x : aResponse)
                sPairs.assign_lower(x.name_string(), x.value());

            return deflate(sPairs.m_Pairs);
        }

        template <class T>
        void inflate(std::string_view aStr, T& aObject)
        {
            while (!aStr.empty()) {
                nghttp2_nv sHeader;
                int        sFlags = 0;

                int sUsed = nghttp2_hd_inflate_hd2(m_Inflate.get(),
                                                   &sHeader,
                                                   &sFlags,
                                                   (const uint8_t*)aStr.data(),
                                                   aStr.size(),
                                                   sHeader.flags & Flags::END_HEADERS);

                if (sUsed < 0)
                    throw std::runtime_error("fail to inflate headers");
                aStr.remove_prefix(sUsed);
                if (sFlags & NGHTTP2_HD_INFLATE_EMIT)
                    collect(sHeader, aObject);
                if (sFlags & NGHTTP2_HD_INFLATE_FINAL)
                    nghttp2_hd_inflate_end_headers(m_Inflate.get());
            }
        }

        template <class T>
        void inflate(const Header& aHeader, std::string_view aData, T& aObject)
        {
            Container::imemstream sData(aData);

            uint8_t sPadLength = 0;
            if (aHeader.flags & Flags::PADDED)
                sData.read(sPadLength);
            if (aHeader.flags & Flags::PRIORITY) {
                sData.skip(4 + 1); // stream id + prio
            }
            std::string_view sRest = sData.rest();
            sRest.remove_suffix(sPadLength);
            TRACE("got headers block of " << sRest.size() << " bytes");
            inflate(sRest, aObject);
        }
    };

    // output queue
    // implement framing and flow control
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
            assert(aData.size() == 4);
            Container::imemstream sData(aData);

            uint32_t sInc = 0;
            sData.read(sInc);
            sInc = be32toh(sInc);
            sInc &= 0x7FFFFFFFFFFFFFFF; // clear R bit

            budget(aHeader.stream, sInc);
        }

        void budget(uint32_t aStreamId, uint32_t aInc)
        {
            if (aStreamId == 0) {
                m_Budget += aInc;
                TRACE("connection window increment " << aInc << ", now " << m_Budget);
            } else {
                auto& sBudget = m_Info[aStreamId].budget;
                sBudget += aInc;
                TRACE("stream window increment " << aInc << ", now " << sBudget);
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

} // namespace asio_http::v2