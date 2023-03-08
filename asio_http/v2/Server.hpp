#pragma once

#ifndef ASIO_HTTP_LIBRARY_HEADER
#include "../v1/Server.hpp"
#include "Output.hpp"
#include "Parser.hpp"
#endif

namespace asio_http::v2 {
#ifdef ASIO_HTTP_LIBRARY_HEADER
    void startServer(asio::io_service& aService, uint16_t aPort, RouterPtr aRouter);
#else
    class Session : public std::enable_shared_from_this<Session>, parser::API
    {
        asio::io_service&        m_Service;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;
        RouterPtr                m_Router;
        std::string              m_PeerName;

        InputBuf     m_Input;
        parser::Main m_Parser;
        Output       m_Output;

        bool legacy(asio::yield_context yield)
        {
            beast::error_code ec;
            m_Stream.expires_after(std::chrono::seconds(30));
            m_Stream.async_read_some(asio::null_buffers(), yield[ec]);
            if (ec)
                throw ec;
            int  sFd     = m_Stream.socket().native_handle();
            char sTmp[3] = {0, 0, 0};
            int  sRet    = ::recv(sFd, sTmp, sizeof(sTmp), MSG_PEEK);
            if (sRet == sizeof(sTmp) and 0 != strncasecmp(sTmp, "PRI", 3))
                return true;
            return false;
        }

        void spawn_write_coro()
        {
            // FIXME: stop read_coro on error
            asio::spawn(m_Strand, [this, p = shared_from_this()](asio::yield_context yield) mutable {
                try {
                    m_Output.coro(yield);
                } catch (const beast::error_code e) {
                    ERROR("beast error: " << e.message());
                } catch (const std::exception& e) {
                    ERROR("exception: " << e.what());
                }
            });
        }

        // parser::API
        parser::MessagePtr new_message(uint32_t aID) override
        {
            return std::make_shared<parser::AsioRequest>();
        }
        void process_message(uint32_t aID, parser::MessagePtr&& aMessage) override
        {
            TRACE("got complete request " << aID);
            auto* sRequest = static_cast<parser::AsioRequest*>(aMessage.get());

            auto sCall = [p = shared_from_this(), aID, aRequest = std::move(sRequest->request)](asio::yield_context yield) mutable {
                beast::error_code ec;
                Response          sResponse;

                Container::Session::Set sPeer("peer", p->m_PeerName);
                p->m_Router->call(p->m_Service, aRequest, sResponse, yield[ec]);
                // FIXME: handle ec errors

                sResponse.prepare_payload();
                if (0 == sResponse.count(http::field::server))
                    sResponse.set(http::field::server, "Beast/cxx");

                p->m_Strand.post([p, aID, sResponse = std::move(sResponse)]() mutable {
                    p->m_Output.enqueue(aID, sResponse);
                });
            };
            // FIXME: why spawn synchronous without post ?
            m_Service.post([p = shared_from_this(), sCall = std::move(sCall)]() {
                asio::spawn(p->m_Service, sCall);
            });
            TRACE("handler spawned");
        }
        void window_update(uint32_t aID, uint32_t aInc) override
        {
            m_Output.process_window_update(aID, aInc);
        }
        void send(std::string&& aBuffer) override
        {
            m_Output.send(std::move(aBuffer));
        }

    public:
        Session(asio::io_service& aService, beast::tcp_stream&& aStream, RouterPtr aRouter)
        : m_Service(aService)
        , m_Strand(m_Service)
        , m_Stream(std::move(aStream))
        , m_Router(aRouter)
        , m_Parser(parser::SERVER, this)
        , m_Output(m_Stream, m_Strand)
        {
        }

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
            CATAPULT_THREAD("server")
            try {
                m_PeerName = m_Stream.socket().remote_endpoint().address().to_string() +
                             ':' +
                             std::to_string(m_Stream.socket().remote_endpoint().port());
                DEBUG("connection from " << m_PeerName);

                if (legacy(yield)) {
                    DEBUG("legacy 1.1 client");
                    v1::session(m_Service, m_Stream, m_Router, yield);
                    return;
                }
                spawn_write_coro();

                beast::error_code ec;
                std::string       sBuffer;
                while (true) {
                    sBuffer.resize(0);
                    sBuffer.reserve(m_Parser.hint());
                    while (true) {
                        if (m_Input.append(sBuffer, m_Parser.hint() - sBuffer.size()))
                            break;
                        m_Stream.expires_after(std::chrono::seconds(30));
                        //CATAPULT_EVENT("input", "async_read_some");
                        size_t sNew = m_Stream.async_read_some(m_Input.buffer(), yield[ec]);
                        if (ec)
                            throw ec;
                        m_Input.push(sNew);
                    }
                    assert(sBuffer.size() == m_Parser.hint());
                    m_Parser.process(sBuffer);
                }
            } catch (const beast::error_code e) {
                ERROR("beast error: " << e.message());
            } catch (const std::exception& e) {
                ERROR("exception: " << e.what());
            }
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

#ifndef ASIO_HTTP_LIBRARY_IMPL
    inline
#endif
        // clang-format off
    void startServer(asio::io_service& aService, uint16_t aPort, RouterPtr aRouter)
    // clang-format on
    {
        auto const sAddress  = asio::ip::make_address("0.0.0.0");
        auto       sAcceptor = std::make_shared<tcp::acceptor>(aService, tcp::endpoint(sAddress, aPort));
        auto       sSocket   = std::make_shared<tcp::socket>(aService);
        server(aService, sAcceptor, sSocket, aRouter);
    }
#endif // ASIO_HTTP_LIBRARY_HEADER
} // namespace asio_http::v2
