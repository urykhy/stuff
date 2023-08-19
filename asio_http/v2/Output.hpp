#pragma once

#include "HPack.hpp"
#include "Types.hpp"

#include <container/Stream.hpp>

namespace asio_http::v2 {

    class Output
    {
        beast::tcp_stream& m_Stream;
        Strand&            m_Strand;
        asio::steady_timer m_Timer;
        Deflate            m_Deflate;

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

#ifdef CATAPULT_PROFILE
        std::optional<Profile::Catapult::Holder> m_LowBudget;
#endif
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
            CATAPULT_COUNTER("output", "streams (before flush)", m_Info.size());
#ifdef CATAPULT_PROFILE
            if (m_LowBudget)
                m_LowBudget.reset();
#endif

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
#ifdef CATAPULT_PROFILE
                    // CATAPULT_EVENT("output", "low connection budget");
                    // m_LowBudget.emplace(std::move(sCatapultHolder));
#endif
                    TRACE("write out: low connection budget");
                    break;
                }
            }
            CATAPULT_COUNTER("output", "connection window (after flush)", m_Budget);
#ifdef CATAPULT_PROFILE
            const size_t sWaitBytes = [this]() {
                size_t sBytes = 0;
                for (auto x = m_Info.begin(); x != m_Info.end(); x++) {
                    sBytes += x->second.body.size();
                }
                return sBytes;
            }();
            CATAPULT_COUNTER("output", "wait bytes (after flush)", sWaitBytes);
#endif
        }

    public:
        Output(beast::tcp_stream& aStream, asio::strand<asio::io_context::executor_type>& aStrand)
        : m_Stream(aStream)
        , m_Strand(aStrand)
        , m_Timer(m_Strand)
        {
            m_Buffer.reserve(MAX_MERGE);
        }

        bool idle() const
        {
            return m_Info.empty() and m_WriteQueue.empty();
        }

        void process_window_update(uint32_t aID, uint32_t aInc)
        {
            if (aID == 0) {
                m_Budget += aInc;
                TRACE("connection window increment " << aInc << ", now " << m_Budget);
                CATAPULT_COUNTER("output", "connection window (update)", m_Budget);
            } else {
                auto& sBudget = m_Info[aID].budget;
                sBudget += aInc;
                TRACE("stream window increment " << aInc << ", now " << sBudget);
            }
            flush();
            m_Timer.cancel();
        }

        void send(std::string&& aBuffer)
        {
            m_PriorityQueue.push_back(std::move(aBuffer));
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
            CATAPULT_COUNTER("output", "streams (enqueue)", m_Info.size())
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
            CATAPULT_COUNTER("output", "streams (enqueue)", m_Info.size())
            m_Timer.cancel();
        }

        void coro(asio::yield_context yield)
        {
            beast::error_code ec;

            auto sWriteOut = [&](std::list<std::string>& aQueue) {
                size_t sLength = 0;
                size_t sCount  = 0;
                for (auto sIt = aQueue.begin();
                     sIt != aQueue.end() and sCount < MAX_MERGE and sLength < MAX_STREAM_EXCLUSIVE;
                     sIt++, sCount++) {
                    auto& sStr = *sIt;
                    m_Buffer.push_back(asio::const_buffer(sStr.data(), sStr.size()));
                    sLength += sStr.size();
                }
                CATAPULT_EVENT("output", "write")
                asio::async_write(m_Stream, m_Buffer, yield[ec]);
                if (ec)
                    throw ec;
                for (size_t i = 0; i < sCount; i++)
                    aQueue.pop_front();
                m_Buffer.clear();
            };

            while (true) {
                if (m_WriteQueue.empty() and m_PriorityQueue.empty()) {
                    using namespace std::chrono_literals;
                    CATAPULT_EVENT("output", "idle")
                    m_Timer.expires_from_now(1s);
                    m_Timer.async_wait(yield[ec]);
                }
                if (!m_PriorityQueue.empty()) {
                    // CATAPULT_COUNTER("output", "priority queue", m_PriorityQueue.size())
                    sWriteOut(m_PriorityQueue);
                    continue;
                }
                if (!m_WriteQueue.empty()) {
                    // CATAPULT_COUNTER("output", "write queue", m_WriteQueue.size())
                    sWriteOut(m_WriteQueue);
                    if (m_Budget > MIN_FRAME_SIZE)
                        flush();
                    TRACE("write out: write_queue size: " << m_WriteQueue.size() << ", queue size: " << m_Info.size());
                }
            }
        }
    };
} // namespace asio_http::v2