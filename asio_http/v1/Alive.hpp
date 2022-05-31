#pragma once

#include <memory>
#include <set>

#include "Client.hpp"

#ifndef ASIO_HTTP_LIBRARY_HEADER
#include <container/Pool.hpp>
#include <container/RequestQueue.hpp>
#include <unsorted/Log4cxx.hpp>
#endif

namespace asio_http::v1 {
#ifdef ASIO_HTTP_LIBRARY_HEADER
    std::shared_ptr<Client> makeClient(asio::io_service& aService);
#else
    static log4cxx::LoggerPtr sLogger = Logger::Get("http");

    class Manager;
    using ManagerPtr = std::shared_ptr<Manager>;

    using Callback = std::function<void()>;

    struct Connection
    {
        const Addr        addr;
        beast::tcp_stream stream;

        Connection(asio::io_service& aService, Addr&& aAddr)
        : addr(std::move(aAddr))
        , stream(aService)
        {
        }

        void close()
        {
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            DEBUG("closed connection " << this << " to " << addr.host << ":" << addr.port);
        }

        ~Connection()
        {
            close();
        }
    };
    using ConnectionPtr = std::shared_ptr<Connection>;

    struct Params
    {
        unsigned max_connections  = 32;
        unsigned queue_timeout_ms = 1000;
    };

    class Manager : public std::enable_shared_from_this<Manager>, public Client
    {
        Container::KeyPool<Addr, ConnectionPtr> m_Alive;
        asio::io_service&                       m_Service;
        asio::deadline_timer                    m_Timer;
        asio::io_service::strand                m_Strand;

        struct RQ
        {
            ClientRequest request;
            Promise       promise;
            Callback      callback;
        };

        const Params                m_Params;
        uint64_t                    m_Current = 0; // number of current requests
        container::RequestQueue<RQ> m_Waiting;     // pending requests + expiration

    public:
        Manager(asio::io_service& aService, const Params& aParams)
        : m_Service(aService)
        , m_Timer(aService)
        , m_Strand(aService)
        , m_Params(aParams)
        , m_Waiting([this](RQ& x) mutable { expired(x); })
        {
        }

        void start_cleaner()
        {
            m_Timer.expires_from_now(boost::posix_time::milliseconds(m_Waiting.eta(1000)));
            m_Timer.async_wait(m_Strand.wrap([this, p = this->shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    DEBUG("close idle connections ...");
                    m_Alive.cleanup();
                    m_Waiting.on_timer();
                    start_cleaner();
                }
            }));
        }

        std::future<Response> async(ClientRequest&& aRequest) override
        {
            auto sPromise = std::make_shared<std::promise<Response>>();
            m_Strand.post([aRequest = std::move(aRequest), sPromise, p = shared_from_this()]() mutable {
                p->async_i({std::move(aRequest), sPromise, {}});
            });
            return sPromise->get_future();
        }

        std::shared_ptr<CT> async_y(ClientRequest&& aRequest, net::yield_context yield) override
        {
            auto sPromise    = std::make_shared<std::promise<Response>>();
            auto sCompletion = std::make_shared<CT>(yield);

            auto sCB = [sCompletion, sPromise, sHandler = sCompletion->completion_handler]() {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(std::bind(sHandler, sPromise), &sHandler);
            };

            m_Strand.post([aRequest = std::move(aRequest), sPromise, sCB, p = shared_from_this()]() mutable {
                p->async_i({std::move(aRequest), sPromise, std::move(sCB)});
            });

            return sCompletion;
        }

    private:
        void async_i(RQ&& aRQ)
        {
            if (m_Current < m_Params.max_connections and m_Waiting.empty()) {
                start(std::move(aRQ));
            } else {
                m_Waiting.insert(std::move(aRQ), m_Params.queue_timeout_ms);
            }
        }

        void start(RQ&& aRQ)
        {
            auto    sParsed   = Parser::url(aRQ.request.url);
            Request sInternal = prepareRequest(aRQ.request, sParsed);
            Addr    sAddr{sParsed.host, sParsed.port};
            auto    sAlive = m_Alive.get(sAddr);
            m_Current++;

            if (sAlive) {
                auto sPtr = *sAlive;
                DEBUG("reuse connection " << sPtr.get() << " to " << sParsed.host << ":" << sParsed.port);
                boost::asio::spawn(m_Strand,
                                   [sInternal = std::move(sInternal),
                                    sRQ       = std::move(aRQ),
                                    sPtr,
                                    p = shared_from_this()](boost::asio::yield_context yield) mutable {
                                       p->perform(std::move(sRQ), sInternal, sPtr, yield);
                                       p->done();
                                       if (sRQ.callback)
                                           sRQ.callback();
                                   });
            } else {
                auto sPtr = std::make_shared<Connection>(m_Service, std::move(sAddr));
                DEBUG("new connection " << sPtr.get() << " to " << sParsed.host << ":" << sParsed.port);
                boost::asio::spawn(m_Strand,
                                   [sInternal = std::move(sInternal),
                                    sRQ       = std::move(aRQ),
                                    sPtr,
                                    p = shared_from_this()](boost::asio::yield_context yield) mutable {
                                       p->create(std::move(sRQ), sInternal, sPtr, yield);
                                       p->done();
                                       if (sRQ.callback)
                                           sRQ.callback();
                                   });
            }
        }

        template <class T>
        static Request prepareRequest(ClientRequest& aRequest, const T& aParsed)
        {
            Request sInternal{aRequest.method, aParsed.path, 11}; // 11 is 1.1 http version

            for (auto& [sField, sValue] : aRequest.headers)
                sInternal.set(sField, std::move(sValue));

            sInternal.body() = std::move(aRequest.body);
            if (!sInternal.body().empty())
                sInternal.prepare_payload();

            return sInternal;
        }

        static void report(RQ& aRQ, ConnectionPtr aPtr, const char* aMsg, beast::error_code aError)
        {
            aRQ.promise->set_exception(std::make_exception_ptr(std::runtime_error(aMsg + aError.message())));
            aPtr->close();
        }

        void create(RQ&& aRQ, Request& aInternal, ConnectionPtr aPtr, net::yield_context yield)
        {
            beast::error_code ec;
            tcp::resolver     sResolver{m_Service};

            aPtr->stream.expires_after(std::chrono::milliseconds(aRQ.request.connect));
            auto const sAddr = sResolver.async_resolve(aPtr->addr.host, aPtr->addr.port, yield[ec]);
            if (ec) {
                report(aRQ, aPtr, "resolve: ", ec);
                return;
            }

            aPtr->stream.async_connect(sAddr, yield[ec]);
            if (ec) {
                report(aRQ, aPtr, "connect: ", ec);
                return;
            }
            aPtr->stream.socket().set_option(tcp::no_delay(true));

            perform(std::move(aRQ), aInternal, aPtr, yield);
        }

        void perform(RQ&& aRQ, Request& aInternal, ConnectionPtr aPtr, net::yield_context yield)
        {
            beast::error_code  ec;
            beast::flat_buffer sBuffer;
            Response           sResponse;

            aPtr->stream.expires_after(std::chrono::milliseconds(aRQ.request.total));
            http::async_write(aPtr->stream, aInternal, yield[ec]);
            if (ec) {
                report(aRQ, aPtr, "write: ", ec);
                return;
            }

            http::async_read(aPtr->stream, sBuffer, sResponse, yield[ec]);
            if (ec) {
                report(aRQ, aPtr, "read: ", ec);
                return;
            }

            aRQ.promise->set_value(std::move(sResponse));

            DEBUG("keep alive connection " << aPtr.get() << " to " << aPtr->addr.host << ":" << aPtr->addr.port);
            m_Alive.insert(aPtr->addr, aPtr);
        }

        void done()
        {
            m_Strand.post([p = shared_from_this()]() mutable {
                p->done_i();
            });
        }

        void done_i()
        {
            // called once request done
            if (m_Current > 0)
                m_Current--;

            auto sNew = m_Waiting.get();
            if (sNew)
                start(std::move(*sNew));
        }

        void expired(RQ& x)
        {
            x.promise->set_exception(std::make_exception_ptr(std::runtime_error("timeout in queue")));
        }
    };

#ifndef ASIO_HTTP_LIBRARY_IMPL
    inline
#endif
        std::shared_ptr<Client>
        makeClient(asio::io_service& aService)
    {
        auto sClient = std::make_shared<asio_http::v1::Manager>(aService, asio_http::v1::Params{});
        sClient->start_cleaner();
        return sClient;
    }
#endif // ASIO_HTTP_LIBRARY_HEADER
} // namespace asio_http::v1
