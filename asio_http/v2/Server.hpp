#pragma once

#include "../v1/Server.hpp"
#include "HPack.hpp"
#include "Input.hpp"
#include "Output.hpp"

#include <threads/Asio.hpp>

namespace asio_http::v2 {

    class Session : public std::enable_shared_from_this<Session>, InputFace
    {
        asio::io_service&        m_Service;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;
        RouterPtr                m_Router;
        Inflate                  m_Inflate;
        Deflate                  m_Deflate;

        std::unique_ptr<CoroState> m_ReadCoro;
        std::unique_ptr<CoroState> m_WriteCoro;
        asio::steady_timer         m_Timer;

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
            m_Output.send(sHeader, m_Deflate(sResponse), m_WriteCoro.get());
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
            TRACE("spawn to perform call for stream " << aStreamId);

            std::string sClientAddr = m_Stream.socket().remote_endpoint().address().to_string();
            auto        sCall       = [p = shared_from_this(), aStreamId, aRequest = std::move(aRequest), sClientAddr = std::move(sClientAddr)](asio::yield_context yield) mutable {
                beast::error_code ec;
                Response          sResponse;

                Container::Session::Set sPeer("peer", sClientAddr);
                p->m_Router->call(p->m_Service, aRequest, sResponse, yield[ec]);
                // FIXME: handle ec

                sResponse.prepare_payload();
                if (0 == sResponse.count(http::field::server))
                    sResponse.set(http::field::server, "Beast/cxx");

                p->m_Strand.post([p, aStreamId, sResponse = std::move(sResponse)]() mutable {
                    p->call_cb(aStreamId, std::move(sResponse));
                });
            };

            // FIXME: why spawn synchronous without post ?
            m_Service.post([p = shared_from_this(), sCall = std::move(sCall)]() {
                asio::spawn(p->m_Service, sCall);
            });

            TRACE("spawned");
        }

        void call_cb(uint32_t aStreamId, Response&& aResponse)
        {
            const bool sStart = m_WriteQueue.empty();
            TRACE("queued response for stream " << aStreamId);
            m_WriteQueue.push_back(ResponseItem{aStreamId, std::move(aResponse)});
            if (sStart and !m_WriteCoro)
                spawn_write_coro();
        }

        void process_settings(const Frame& aFrame) override
        {
            if (aFrame.header.flags != 0) // filter out ACK_SETTINGS
                return;

            Container::imemstream sData(aFrame.body);
            while (!sData.eof()) {
                SettingVal sVal;
                sData.read(sVal);
                sVal.to_host();
                TRACE(sVal.key << ": " << sVal.value);
            }
            Header sAck;
            sAck.type  = Type::SETTINGS;
            sAck.flags = Flags::ACK_SETTINGS;
            m_Output.send(sAck, {}, m_ReadCoro.get());
            TRACE("ack settings");
        }

        void process_window_update(const Frame& aFrame) override
        {
            m_Output.window_update(aFrame.header, aFrame.body);
        }

        void process_request(uint32_t aStreamId, Request&& aRequest) override
        {
            TRACE("got complete request");
            call(aStreamId, std::move(aRequest));
        }

        bool legacy()
        {
            m_Stream.expires_after(std::chrono::seconds(30));
            m_Stream.async_read_some(asio::null_buffers(), m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;
            int  sFd     = m_Stream.socket().native_handle();
            char sTmp[3] = {0, 0, 0};
            int  sRet    = ::recv(sFd, sTmp, sizeof(sTmp), MSG_PEEK);
            if (sRet == sizeof(sTmp) and 0 != strncasecmp(sTmp, "PRI", 3))
                return true;
            return false;
        }

        void hello()
        {
            const std::string_view sExpected("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
            TRACE("wait for preface");
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

    public:
        Session(asio::io_service& aService, beast::tcp_stream&& aStream, RouterPtr aRouter)
        : m_Service(aService)
        , m_Strand(m_Service)
        , m_Stream(std::move(aStream))
        , m_Router(aRouter)
        , m_Timer(m_Service)
        , m_Output(m_Stream)
        , m_Input(m_Stream, this, true /* server */)
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
                DEBUG("connection from " << m_Stream.socket().remote_endpoint());
                m_ReadCoro = std::make_unique<CoroState>(CoroState{{}, yield});
                if (legacy()) {
                    DEBUG("legacy 1.1 client");
                    v1::session(m_Service, m_Stream, m_Router, yield);
                    return;
                }
                m_Input.assign(m_ReadCoro.get());
                hello();
                TRACE("http/2 negotiated");

                while (true) {
                    m_Input.process_frame();
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
            TRACE("write out coro started");
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

            TRACE("write out coro finished");
            m_WriteCoro.reset();
            m_Output.assign(nullptr);
        }
    };

    inline void server(asio::io_service& aService, std::shared_ptr<tcp::acceptor> aAcceptor, std::shared_ptr<tcp::socket> aSocket, RouterPtr aRouter)
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
            server(aService, aAcceptor, aSocket, aRouter);
        });
    }

    inline void startServer(Threads::Asio& aContext, uint16_t aPort, RouterPtr aRouter)
    {
        auto const sAddress  = asio::ip::make_address("0.0.0.0");
        auto       sAcceptor = std::make_shared<tcp::acceptor>(aContext.service(), tcp::endpoint(sAddress, aPort));
        auto       sSocket   = std::make_shared<tcp::socket>(aContext.service());
        server(aContext.service(), sAcceptor, sSocket, aRouter);
    }
} // namespace asio_http::v2
