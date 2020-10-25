#pragma once

#include <set>

#include <container/Pool.hpp>

#include "Client.hpp"

namespace asio_http::Alive {

    class Manager;
    using ManagerPtr = std::shared_ptr<Manager>;

    struct Connection
    {
        struct Peer
        {
            std::string host;
            std::string port;

            auto as_tuple() const
            {
                return std::tie(host, port);
            }
            bool operator<(const Peer& aOther) const
            {
                return as_tuple() < aOther.as_tuple();
            }
        };

        static inline std::atomic_uint64_t m_Serial{0};

        const Peer        peer;
        const uint64_t    serial;
        beast::tcp_stream stream;
        Promise           promise;
        ManagerPtr        manager;

        Connection(asio::io_service& aService, Peer&& aPeer)
        : peer(std::move(aPeer))
        , serial(m_Serial++)
        , stream(aService)
        {
        }

        void report(const char* aMsg, beast::error_code aError)
        {
            promise->set_exception(std::make_exception_ptr(std::runtime_error(aMsg + aError.message())));
            promise.reset();
            close();
        }

        void close()
        {
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        }

        ~Connection()
        {
            close();
        }
    };
    using ConnectionPtr = std::shared_ptr<Connection>;

    class Manager : public std::enable_shared_from_this<Manager>
    {
        Container::KeyPool<Connection::Peer, ConnectionPtr> m_Alive;
        asio::io_service&                                   m_Service;
        asio::deadline_timer                                m_Timer;
        asio::io_service::strand                            m_Strand;

    public:
        Manager(asio::io_service& aService)
        : m_Service(aService)
        , m_Timer(aService)
        , m_Strand(aService)
        {
        }

        void start_cleaner()
        {
            m_Timer.expires_from_now(boost::posix_time::seconds(1));
            m_Timer.async_wait(m_Strand.wrap([this, p = this->shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    m_Alive.cleanup();
                    start_cleaner();
                }
            }));
        }

        std::future<Response> async(ClientRequest&& aRequest)
        {
            auto sPromise = std::make_shared<std::promise<Response>>();
            m_Strand.post([aRequest = std::move(aRequest), sPromise, p = shared_from_this()]() mutable {
                p->async_i(std::move(aRequest), sPromise);
            });
            return sPromise->get_future();
        }

    private:
        void async_i(ClientRequest&& aRequest, Promise aPromise)
        {
            auto             sParsed = Parser::url(aRequest.url);
            Connection::Peer sPeer{std::move(sParsed.host), std::move(sParsed.port)};
            auto             sAlive = m_Alive.get(sPeer);

            if (sAlive) {
                auto sPtr     = *sAlive;
                sPtr->promise = aPromise;
                boost::asio::spawn(m_Strand, [aRequest = std::move(aRequest), sPtr](boost::asio::yield_context yield) mutable {
                    auto    sParsed   = Parser::url(aRequest.url);
                    Request sInternal = prepareRequest(aRequest, sParsed);
                    perform(std::move(aRequest), sInternal, sPtr, yield);
                });
            } else {
                auto sPtr     = std::make_shared<Connection>(m_Service, std::move(sPeer));
                sPtr->promise = aPromise;
                sPtr->manager = shared_from_this();

                boost::asio::spawn(m_Strand, [aRequest = std::move(aRequest), sPtr](boost::asio::yield_context yield) mutable {
                    start(sPtr->manager->m_Service, std::move(aRequest), sPtr, yield);
                });
            }
        }

        template <class T>
        static Request prepareRequest(ClientRequest& aRequest, const T& aParsed)
        {
            Request sInternal{aRequest.method, aParsed.query, 11}; // 11 is 1.1 http version

            for (auto& [sField, sValue] : aRequest.headers)
                sInternal.set(sField, std::move(sValue));

            sInternal.body() = std::move(aRequest.body);
            if (!sInternal.body().empty())
                sInternal.prepare_payload();

            return sInternal;
        }

        static void start(asio::io_service& aService, ClientRequest&& aRequest, ConnectionPtr aPtr, net::yield_context yield)
        {
            auto    sParsed   = Parser::url(aRequest.url);
            Request sInternal = prepareRequest(aRequest, sParsed);

            beast::error_code ec;
            tcp::resolver     sResolver{aService};

            aPtr->stream.expires_after(std::chrono::milliseconds(aRequest.connect));
            auto const sAddr = sResolver.async_resolve(sParsed.host, sParsed.port, yield[ec]);
            if (ec) {
                aPtr->report("resolve: ", ec);
                return;
            }

            aPtr->stream.async_connect(sAddr, yield[ec]);
            if (ec) {
                aPtr->report("connect: ", ec);
                return;
            }

            perform(std::move(aRequest), sInternal, aPtr, yield);
        }

        static void perform(ClientRequest&& aRequest, Request& aInternal, ConnectionPtr aPtr, net::yield_context yield)
        {
            beast::error_code  ec;
            beast::flat_buffer sBuffer;
            Response           sResponse;

            aPtr->stream.expires_after(std::chrono::milliseconds(aRequest.total));
            http::async_write(aPtr->stream, aInternal, yield[ec]);
            if (ec) {
                aPtr->report("write: ", ec);
                return;
            }

            http::async_read(aPtr->stream, sBuffer, sResponse, yield[ec]);
            if (ec) {
                aPtr->report("read: ", ec);
                return;
            }

            aPtr->promise->set_value(std::move(sResponse));
            aPtr->promise.reset();
            aPtr->manager->m_Alive.insert(aPtr->peer, aPtr);
        }
    };

} // namespace asio_http::Alive