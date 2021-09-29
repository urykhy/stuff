#pragma once

#include <cassert>
#include <map>
#include <memory>

#include "Router.hpp"
#include "Types.hpp"

#include <string/String.hpp>
#include <threads/Asio.hpp>
#include <unsorted/Log4cxx.hpp>

namespace asio_http::v2 {

    class Session : public std::enable_shared_from_this<Session>
    {
        asio::io_service&        m_Service;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;
        RouterPtr                m_Router;
        HPack                    m_Pack;

        std::unique_ptr<CoroState> m_ReadCoro;
        std::unique_ptr<CoroState> m_WriteCoro;
        asio::steady_timer         m_Timer;

        // recv streams
        struct Stream
        {
            Request m_Request;
            bool    m_NoBody = false;
        };
        std::map<uint32_t, Stream> m_Streams;

        // responses to send
        struct ResponseItem
        {
            uint32_t stream_id;
            Response response;
        };
        std::list<ResponseItem> m_WriteQueue;

        Output m_Output;
        Input  m_Input;

        void send(ResponseItem&& aResponse)
        {
            auto& sResponse = aResponse.response;

            Header sHeader;
            sHeader.type   = Type::HEADERS;
            sHeader.flags  = Flags::END_HEADERS;
            sHeader.stream = aResponse.stream_id;

            if (sResponse.body().empty())
                sHeader.flags |= Flags::END_STREAM;
            m_Output.send(sHeader, m_Pack.deflate(sResponse), m_WriteCoro.get());
            if (!sResponse.body().empty())
                m_Output.enqueue(aResponse.stream_id, std::move(sResponse.body()));
        }

        void flush()
        {
            while (!m_WriteQueue.empty()) {
                send(std::move(m_WriteQueue.front()));
                m_WriteQueue.pop_front();
            }
            m_Output.flush();
        }

        //

        void call(uint32_t aStreamId, Request&& aRequest)
        {
            DEBUG("spawn to perform call for stream " << aStreamId);

            auto sCall = [p=shared_from_this(), aStreamId, aRequest = std::move(aRequest)](asio::yield_context yield) mutable {
                beast::error_code ec;
                Response          sResponse;

                p->m_Router->call(p->m_Service, aRequest, sResponse, yield[ec]);

                sResponse.prepare_payload();
                if (0 == sResponse.count(http::field::server))
                    sResponse.set(http::field::server, "Beast/cxx");

                p->m_Strand.post([p, aStreamId, sResponse = std::move(sResponse)]() mutable {
                    p->call_cb(aStreamId, std::move(sResponse));
                });
            };

            // FIXME: spawn synchronous without post
            m_Service.post([p = shared_from_this(), sCall = std::move(sCall)]() {
                asio::spawn(p->m_Service, sCall);
            });

            DEBUG("spawned");
        }

        void call_cb(uint32_t aStreamId, Response&& aResponse)
        {
            const bool sStart = m_WriteQueue.empty();
            DEBUG("queued response for stream " << aStreamId);
            m_WriteQueue.push_back(ResponseItem{aStreamId, std::move(aResponse)});
            if (sStart and !m_WriteCoro)
                spawn_write_coro();
        }

        // Wire protocol

        void process_data(const Input::Frame& aFrame)
        {
            const uint32_t sStreamId = aFrame.header.stream;
            assert(sStreamId != 0);
            auto sIt = m_Streams.find(sStreamId);
            assert(sIt != m_Streams.end());

            m_Input.append(sStreamId, aFrame.body, aFrame.header.flags ^ Flags::END_STREAM);

            if (aFrame.header.flags & Flags::END_STREAM) {
                Request& sRequest = sIt->second.m_Request;
                sRequest.body().assign(m_Input.extract(sStreamId));
                DEBUG("got complete request")
                call(sStreamId, std::move(sRequest));
                m_Streams.erase(sIt);
            }
        }

        void process_headers(const Input::Frame& aFrame)
        {
            const uint32_t sStreamId = aFrame.header.stream;
            assert(sStreamId != 0);
            // TODO: if continuation -> assert we already have seen headers frame
            auto& sStream  = m_Streams[sStreamId];
            auto& sRequest = sStream.m_Request;

            m_Pack.inflate(aFrame.header, aFrame.body, sRequest);

            if (aFrame.header.flags & Flags::END_STREAM)
                sStream.m_NoBody = true;
            if (aFrame.header.flags & Flags::END_HEADERS and sStream.m_NoBody) {
                DEBUG("got complete request")
                call(sStreamId, std::move(sRequest));
                m_Streams.erase(sStreamId);
            }
        }

        void process_settings(const Input::Frame& aFrame)
        {
            if (aFrame.header.flags != 0) // filter out ACK_SETTINGS
                return;

            Container::imemstream sData(aFrame.body);
            while (!sData.eof()) {
                SettingVal sVal;
                sData.read(sVal);
                sVal.to_host();
                DEBUG(sVal.key << ": " << sVal.value);
            }
            Header sAck;
            sAck.type  = Type::SETTINGS;
            sAck.flags = Flags::ACK_SETTINGS;
            m_Output.send(sAck, {}, m_ReadCoro.get());
            DEBUG("ack settings");
        }

        void process_window_update(const Input::Frame& aFrame)
        {
            m_Output.window_update(aFrame.header, aFrame.body);
        }

        void hello()
        {
            const std::string_view sExpected("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
            DEBUG("wait for preface");
            m_Stream.expires_after(std::chrono::seconds(30));

            std::string sTmp;
            sTmp.resize(sExpected.size());
            asio::async_read(m_Stream, asio::buffer(sTmp.data(), sTmp.size()), m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;
            if (sTmp != sExpected)
                throw std::runtime_error("not a http/2 peer");

            Header sAck;
            sAck.type = Type::SETTINGS;
            m_Output.send(sAck, {}, m_ReadCoro.get());
        }

        bool process_frame()
        {
            m_Stream.expires_after(std::chrono::seconds(30));
            auto sFrame = m_Input.recv();

            switch (sFrame.header.type) {
            case Type::DATA: process_data(sFrame); break;
            case Type::HEADERS: process_headers(sFrame); break;
            case Type::SETTINGS: process_settings(sFrame); break;
            case Type::GOAWAY: return false; break;
            case Type::WINDOW_UPDATE: process_window_update(sFrame); break;
            case Type::CONTINUATION: process_headers(sFrame); break;
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
        , m_Timer(m_Service)
        , m_Output(m_Stream)
        , m_Input(m_Stream)
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
                m_Input.assign(m_ReadCoro.get());
                hello();
                DEBUG("http/2 negotiated");

                while (true) {
                    if (!process_frame())
                        break; // GO AWAY
                    if (m_WriteCoro and m_WriteCoro->ec)
                        throw m_WriteCoro->ec;
                }
            } catch (const beast::error_code e) {
                ERROR("beast error: " << e.message());
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
                    ERROR("beast error: " << e.message());
                }
            });
        }

        void write_coro(asio::yield_context yield)
        {
            DEBUG("write out coro started");
            m_WriteCoro = std::make_unique<CoroState>(CoroState{{}, yield});
            m_Output.assign(m_WriteCoro.get());

            auto sEnd = [this]() {
                return m_ReadCoro->ec or (m_WriteQueue.empty() and m_Output.idle());
            };
            while (!sEnd()) {
                m_Timer.expires_from_now(std::chrono::milliseconds(1));
                m_Timer.async_wait(m_WriteCoro->yield[m_WriteCoro->ec]);
                flush();
            }

            DEBUG("write out coro finished");
            m_WriteCoro.reset();
            m_Output.assign(nullptr);
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