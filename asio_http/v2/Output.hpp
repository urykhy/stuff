#pragma once

#include "HPack.hpp"
#include "Types.hpp"

#include <container/Stream.hpp>

namespace asio_http::v2 {

    class Output
    {
        beast::tcp_stream&        m_Stream;
        asio::io_service::strand& m_Strand;
        asio::steady_timer        m_Timer;
        Deflate                   m_Deflate;

        // send queue for not accountable frames (or we have budget)
        std::list<std::string> m_PriorityQueue;
        std::list<std::string> m_WriteQueue;

        // responses body to send
        struct Info
        {
            std::string body;
            uint32_t    budget = DEFAULT_WINDOW_SIZE;
        };
        std::map<uint32_t, Info> m_Info;

        uint32_t m_Budget = DEFAULT_WINDOW_SIZE; // connection budget

        enum
        {
            MAX_MERGE = 16
        };
        std::vector<asio::const_buffer> m_Buffer;

        //std::optional<Profile::Catapult::Holder> m_LowBudget;

        void enqueue(uint32_t aStreamId, std::string_view aData, bool aLast)
        {
            while (!aData.empty()) {
                const size_t sLen = std::min(aData.size(), DEFAULT_MAX_FRAME_SIZE);

                Header sHeader;
                sHeader.type = Type::DATA;
                if (sLen == aData.size() and aLast)
                    sHeader.flags = Flags::END_STREAM;
                sHeader.stream = aStreamId;
                sHeader.size   = sLen;
                sHeader.to_net();

                std::string sTmp;
                sTmp.append((const char*)&sHeader, sizeof(sHeader));
                sTmp.append(aData.substr(0, sLen));
                m_WriteQueue.push_back(std::move(sTmp));

                aData.remove_prefix(sLen);
            }
        }

        void flush()
        {
            //g_Profiler.counter("output", "streams", m_Info.size());
            //auto sHolder = g_Profiler.start("output", "flush");
            //if (m_LowBudget)
            //    m_LowBudget.reset();

            for (auto x = m_Info.begin(); x != m_Info.end();) {
                auto& sStreamId = x->first;
                auto& sData     = x->second;
                auto& sBody     = sData.body;

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
                    TRACE("low budget for stream " << sStreamId << " (" << sBody.size() << " bytes pending)"
                                                   << "(connection: " << m_Budget << ", stream: " << sData.budget << ")");
                    x++;
                    continue;
                }

                const bool sLast = sLen == sBody.size();
                enqueue(sStreamId, std::string_view(sBody.data(), sLen), sLast);
                m_Budget -= sLen;
                TRACE("enqueue " << sLen << " bytes for stream " << sStreamId << ", connection budget: " << m_Budget);

                if (sLast) {
                    TRACE("stream " << sStreamId << " done");
                    x = m_Info.erase(x);
                } else {
                    sData.budget -= sLen;
                    sBody.erase(0, sLen);
                    TRACE("stream " << sStreamId << " have " << sBody.size() << " bytes pending");
                    x++;
                }
                if (m_Budget < MIN_FRAME_SIZE) {
                    //m_LowBudget.emplace(g_Profiler.start("output", "low connection budget"));
                    TRACE("write out: low connection budget");
                    break;
                }
            }
        }

    public:
        Output(beast::tcp_stream& aStream, asio::io_service::strand& aStrand)
        : m_Stream(aStream)
        , m_Strand(aStrand)
        , m_Timer(aStream.get_executor())
        {
            m_Buffer.reserve(MAX_MERGE);
        }

        bool idle() const
        {
            return m_Info.empty() and m_WriteQueue.empty();
        }

        void recv_window_update(const Header& aHeader, const std::string& aData)
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

            flush();
            m_Timer.cancel();
        }

        // input stream requests window
        void emit_window_update(uint32_t aStreamId, uint32_t aInc)
        {
            aInc = htobe32(DEFAULT_WINDOW_SIZE); // to network order
            Header sHeader;
            sHeader.type   = Type::WINDOW_UPDATE;
            sHeader.stream = aStreamId;
            sHeader.size   = sizeof(aInc);
            sHeader.to_net();

            std::string sTmp;
            sTmp.append((const char*)&sHeader, sizeof(sHeader));
            sTmp.append((const char*)&aInc, sizeof(aInc));
            m_PriorityQueue.push_back(std::move(sTmp));

            TRACE("queued window update for stream " << aStreamId);
            m_Timer.cancel();
        }

        void enqueueSettings(bool sAck)
        {
            Header sHeader;
            sHeader.type = Type::SETTINGS;
            if (sAck)
                sHeader.flags = Flags::ACK_SETTINGS;
            sHeader.to_net();

            std::string sTmp;
            sTmp.append((const char*)&sHeader, sizeof(sHeader));
            m_WriteQueue.push_back(std::move(sTmp));
            m_Timer.cancel();
        }

        void enqueue(uint32_t aStreamId, Response& aResponse)
        {
            Header sHeader;
            sHeader.type   = Type::HEADERS;
            sHeader.flags  = Flags::END_HEADERS;
            sHeader.stream = aStreamId;
            if (aResponse.body().empty())
                sHeader.flags |= Flags::END_STREAM;
            auto sDeflated = m_Deflate(aResponse);
            sHeader.size   = sDeflated.size();
            sHeader.to_net();

            std::string sTmp;
            sTmp.append((const char*)&sHeader, sizeof(sHeader));
            sTmp.append(sDeflated);
            m_WriteQueue.push_back(std::move(sTmp));

            if (!aResponse.body().empty()) {
                auto& sInfo = m_Info[aStreamId];
                sInfo.body  = std::move(aResponse.body());
            }

            TRACE("queued response for stream " << aStreamId);
            //g_Profiler.counter("output", "streams", m_Info.size());
            m_Timer.cancel();
        }

        void enqueue(uint32_t aStreamId, ClientRequest& aRequest)
        {
            Header sHeader;
            sHeader.type   = Type::HEADERS;
            sHeader.flags  = Flags::END_HEADERS;
            sHeader.stream = aStreamId;
            if (aRequest.body.empty())
                sHeader.flags |= Flags::END_STREAM;
            auto sDeflated = m_Deflate(aRequest);
            sHeader.size   = sDeflated.size();
            sHeader.to_net();

            std::string sTmp;
            sTmp.append((const char*)&sHeader, sizeof(sHeader));
            sTmp.append(sDeflated);
            m_WriteQueue.push_back(std::move(sTmp));

            if (!aRequest.body.empty()) {
                auto& sInfo = m_Info[aStreamId];
                sInfo.body  = std::move(aRequest.body);
            }

            TRACE("queued request for stream " << aStreamId);
            //g_Profiler.counter("output", "streams", m_Info.size());
            m_Timer.cancel();
        }

        void coro(asio::yield_context yield)
        {
            beast::error_code ec;

            while (true) {
                if (m_WriteQueue.empty()) {
                    using namespace std::chrono_literals;
                    //auto sHolder = g_Profiler.start("output", "idle");
                    m_Timer.expires_from_now(1ms);
                    m_Timer.async_wait(yield[ec]);
                }
                if (!m_PriorityQueue.empty())
                    m_WriteQueue.splice(m_WriteQueue.begin(), m_PriorityQueue, m_PriorityQueue.begin(), m_PriorityQueue.end());
                if (!m_WriteQueue.empty()) {
                    {
                        //auto sHolder = g_Profiler.start("output", "write");
                        size_t sLength = 0;
                        size_t sCount  = 0;
                        for (auto sIt = m_WriteQueue.begin();
                             sIt != m_WriteQueue.end() and sCount < MAX_MERGE and sLength < MAX_STREAM_EXCLUSIVE;
                             sIt++, sCount++) {
                            auto& sStr = *sIt;
                            m_Buffer.push_back(asio::const_buffer(sStr.data(), sStr.size()));
                            sLength += sStr.size();
                        }
                        asio::async_write(m_Stream, m_Buffer, yield[ec]);
                        if (ec)
                            throw ec;
                        for (size_t i = 0; i < sCount; i++)
                            m_WriteQueue.pop_front();
                        m_Buffer.clear();
                    }
                    //g_Profiler.counter("output", "queue", m_WriteQueue.size());
                    if (m_Budget > MIN_FRAME_SIZE)
                        flush();
                    TRACE("write out: write_queue size: " << m_WriteQueue.size() << ", body_queue size: " << m_Info.size());
                }
            }
        }
    };
} // namespace asio_http::v2