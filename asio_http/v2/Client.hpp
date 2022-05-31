#pragma once

#ifndef ASIO_HTTP_LIBRARY_HEADER
#include "Input.hpp"
#include "Output.hpp"
#endif

#include <unsorted/Raii.hpp>

namespace asio_http::v2 {
#ifdef ASIO_HTTP_LIBRARY_HEADER
    std::shared_ptr<Client> makeClient(asio::io_service& aService);
#else
    struct Peer : public std::enable_shared_from_this<Peer>, InputFace
    {
        using Notify = std::function<void(const std::string&)>;

    private:
        asio::io_service&        m_Service;
        const std::string        m_Host;
        const std::string        m_Port;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;

        std::unique_ptr<CoroState> m_ReadCoro;
        uint32_t                   m_Serial = 1;

        // pending requests
        struct PendingStream
        {
            ClientRequest request;
            Promise       promise;
        };
        std::list<PendingStream> m_Pending;

        // active streams
        std::map<uint32_t, Promise> m_Streams;

        Input       m_Input;
        Output      m_Output;
        Notify      m_Notify;
        std::string m_FailReason;

        void fail(const std::string& aMsg)
        {
            ERROR(aMsg);
            if (m_FailReason.empty()) {
                m_FailReason = aMsg;
                if (m_Notify)
                    m_Notify(m_FailReason);
            }
            auto sPtr = std::make_exception_ptr(std::runtime_error(m_FailReason));
            for (auto& [sId, sPromise] : m_Streams)
                sPromise->set_exception(sPtr);
            m_Streams.clear();
            for (auto& x : m_Pending)
                x.promise->set_exception(sPtr);
            m_Pending.clear();
            m_Stream.cancel();
        }

        void fail(const std::exception& e)
        {
            fail(std::string("exception: ") + e.what());
        }

        void fail(const beast::error_code& e)
        {
            fail(std::string("beast error: ") + e.message());
        }

        //

        void async_i(ClientRequest& aRequest, Promise aPromise)
        {
            if (!m_FailReason.empty()) {
                aPromise->set_exception(std::make_exception_ptr(std::runtime_error(m_FailReason)));
                return;
            }
            if (m_Streams.size() >= CONCURRENT_STREAMS) {
                m_Pending.push_back({std::move(aRequest), aPromise});
                return;
            }
            initiate(aRequest, aPromise);
        }

        void initiate(ClientRequest& aRequest, Promise aPromise)
        {
            const uint32_t sStreamId = m_Serial;
            m_Serial += 2; // odd-numbered stream identifiers

            TRACE("create stream " << sStreamId << " for " << aRequest.url);
            m_Output.enqueue(sStreamId, aRequest);
            m_Streams[sStreamId] = aPromise;
        }

        void process_settings(const Frame& aFrame) override
        {
            if (aFrame.header.flags != 0) // filter out ACK_SETTINGS
                return;
            m_Output.enqueueSettings(true);
            TRACE("ack settings");
        }

        void process_window_update(const Frame& aFrame) override
        {
            CATAPULT_MARK("client", "recv window update");
            m_Output.recv_window_update(aFrame.header, aFrame.body);
        }

        void emit_window_update(uint32_t aStreamId, uint32_t aInc) override
        {
            CATAPULT_MARK("client", "emit window update");
            m_Output.emit_window_update(aStreamId, aInc);
        }

        void process_response(uint32_t aStreamId, Response&& aResponse) override
        {
            TRACE("got complete response");

            auto sIt = m_Streams.find(aStreamId);
            assert(sIt != m_Streams.end());
            sIt->second->set_value(std::move(aResponse));
            m_Streams.erase(sIt);

            if (!m_Pending.empty()) {
                auto& [sRequest, sPromise] = m_Pending.front();
                initiate(sRequest, sPromise);
                m_Pending.pop_front();
            }
        }

        void connect()
        {
            tcp::resolver sResolver{m_Service};
            m_Stream.expires_after(std::chrono::milliseconds(100));

            auto const sAddr = sResolver.async_resolve(m_Host, m_Port, m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;

            m_Stream.async_connect(sAddr, m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;
            m_Stream.socket().set_option(tcp::no_delay(true));
        }

        void spawn_write_coro()
        {
            asio::spawn(m_Strand, [this, p = shared_from_this()](asio::yield_context yield) mutable {
                try {
                    m_Output.coro(yield);
                } catch (const beast::error_code e) {
                    fail(e);
                } catch (const std::exception& e) {
                    fail(e);
                }
            });
        }

        void hello()
        {
            const std::string_view sRequest("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
            m_Stream.expires_after(std::chrono::seconds(3));
            asio::async_write(m_Stream, asio::const_buffer(sRequest.data(), sRequest.size()), m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;
            TRACE("sent http/2 connection preface");
            m_Output.enqueueSettings(false);

            auto sFrame = m_Input.recv();
            if (sFrame.header.type != Type::SETTINGS)
                throw std::runtime_error("not a http/2 peer");
            process_settings(sFrame);
        }

    public:
        Peer(asio::io_service& aService, const std::string aHost, const std::string& aPort, Notify&& aNotify = {})
        : m_Service(aService)
        , m_Host(aHost)
        , m_Port(aPort)
        , m_Strand(m_Service)
        , m_Stream(m_Service)
        , m_Input(m_Stream, this, false /* client */)
        , m_Output(m_Stream, m_Strand)
        , m_Notify(std::move(aNotify))
        {
        }

        ~Peer()
        {
            close();
        }

        void close()
        {
            beast::error_code ec;
            m_Stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        }

        void start()
        {
            boost::asio::spawn(m_Strand, [p = shared_from_this()](asio::yield_context yield) mutable {
                p->read_coro(yield);
            });
        }

        std::future<Response> async(ClientRequest&& aRequest)
        {
            auto sPromise = std::make_shared<std::promise<Response>>();
            m_Strand.post([aRequest = std::move(aRequest), sPromise, p = shared_from_this()]() mutable {
                p->async_i(aRequest, sPromise);
            });
            return sPromise->get_future();
        }

        void async(ClientRequest&& aRequest, Promise aPromise)
        {
            m_Strand.post([aRequest = std::move(aRequest), aPromise, p = shared_from_this()]() mutable {
                p->async_i(aRequest, aPromise);
            });
        }

    private:
        void read_coro(asio::yield_context yield)
        {
            CATAPULT_THREAD("client")
            try {
                m_ReadCoro = std::make_unique<CoroState>(CoroState{{}, yield});
                m_Input.assign(m_ReadCoro.get());
                connect();
                DEBUG("connected to " << m_Host << ':' << m_Port);
                spawn_write_coro();
                hello();
                TRACE("http/2 negotiated");
                if (m_Notify)
                    m_Notify("connected");
                while (m_FailReason.empty())
                    m_Input.process_frame();
                // FIXME: check write error
            } catch (const beast::error_code e) {
                fail(e);
            } catch (const std::exception& e) {
                fail(e);
            }
        }
    };

    struct Params
    {
        // unsigned max_connections  = 32;
        // time_t   delay            = 1;
    };

    class Manager : public std::enable_shared_from_this<Manager>, public Client
    {
        asio::io_service&        m_Service;
        asio::deadline_timer     m_Timer;
        asio::io_service::strand m_Strand;
        const Params             m_Params;

        struct RQ
        {
            ClientRequest request;
            Promise       promise;
        };

        struct Data
        {
            std::shared_ptr<Peer> peer;
            std::string           status = "not connected";
            std::list<RQ>         requests;
        };
        using DataPtr = std::shared_ptr<Data>;
        using WeakPtr = std::weak_ptr<Data>;
        std::map<Addr, DataPtr> m_Data;

    public:
        Manager(asio::io_service& aService, const Params& aParams)
        : m_Service(aService)
        , m_Timer(aService)
        , m_Strand(aService)
        , m_Params(aParams)
        {
        }

        void start_cleaner()
        {
            m_Timer.expires_from_now(boost::posix_time::milliseconds(1000));
            m_Timer.async_wait(m_Strand.wrap([this, p = this->shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    // TODO: close failed connections too (once params.delay passed)
                    TRACE("close idle connections ...");
                    start_cleaner();
                }
            }));
        }

        std::future<Response> async(ClientRequest&& aRequest) override
        {
            auto sPromise = std::make_shared<std::promise<Response>>();
            m_Strand.post([aRequest = std::move(aRequest), sPromise, p = shared_from_this()]() mutable {
                p->async_i({std::move(aRequest), sPromise});
            });
            return sPromise->get_future();
        }

        std::shared_ptr<CT> async_y(ClientRequest&& aRequest, net::yield_context yield) override
        {
            throw std::invalid_argument("async_y not implemented");
        }

    private:
        void async_i(RQ&& aRQ)
        {
            auto sParsed = Parser::url(aRQ.request.url);
            Addr sAddr{sParsed.host, sParsed.port};
            auto sIter = m_Data.find(sAddr);
            if (sIter == m_Data.end()) {
                // TODO: check connection size limit -> set error to promise
                auto    sDataPtr = std::make_shared<Data>();
                WeakPtr sWeakPtr = sDataPtr;
                auto    sPeer    = std::make_shared<Peer>(
                    m_Service,
                    sAddr.host,
                    sAddr.port,
                    m_Strand.wrap([sWeakPtr, p = shared_from_this()](const std::string& aMsg) {
                        p->notify_i(sWeakPtr, aMsg);
                          }));
                sDataPtr->peer = sPeer;
                sPeer->start();
                auto sTmp = m_Data.insert(std::make_pair(sAddr, sDataPtr));
                assert(sTmp.second);
                sIter = sTmp.first;
            }
            auto& sData   = sIter->second;
            auto& sStatus = sData->status;
            if (sStatus == "not connected") {
                sData->requests.push_back(std::move(aRQ));
            } else if (sStatus == "connected") {
                sData->peer->async(std::move(aRQ.request), aRQ.promise);
            } else {
                aRQ.promise->set_exception(std::make_exception_ptr(std::runtime_error(sData->status)));
            }
        }

        void notify_i(const WeakPtr aPtr, const std::string& aMessage)
        {
            auto sPtr = aPtr.lock();
            if (sPtr) {
                sPtr->status = aMessage;
                if (aMessage == "connected")
                    on_connected(sPtr);
                else
                    on_error(sPtr);
                sPtr->requests.clear();
            }
        }

        void on_connected(DataPtr aPtr)
        {
            for (auto& x : aPtr->requests)
                aPtr->peer->async(std::move(x.request), x.promise);
        }

        void on_error(DataPtr aPtr)
        {
            for (auto& x : aPtr->requests)
                x.promise->set_exception(std::make_exception_ptr(std::runtime_error(aPtr->status)));
        }
    };

#ifndef ASIO_HTTP_LIBRARY_IMPL
    inline
#endif
        std::shared_ptr<Client>
        makeClient(asio::io_service& aService)
    {
        auto sClient = std::make_shared<Manager>(aService, asio_http::v2::Params{});
        sClient->start_cleaner();
        return sClient;
    }
#endif // ASIO_HTTP_LIBRARY_HEADER
} // namespace asio_http::v2