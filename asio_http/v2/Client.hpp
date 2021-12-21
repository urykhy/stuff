#pragma once

#include "HPack.hpp"
#include "Input.hpp"
#include "Output.hpp"

#include <unsorted/Raii.hpp>

namespace asio_http::v2 {

    struct Peer : public std::enable_shared_from_this<Peer>, InputFace
    {
        using Notify = std::function<void(const std::string&)>;

    private:
        asio::io_service&        m_Service;
        const std::string        m_Host;
        const std::string        m_Port;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;
        Inflate                  m_Inflate;
        Deflate                  m_Deflate;

        std::unique_ptr<CoroState> m_ReadCoro;
        std::unique_ptr<CoroState> m_WriteCoro;
        asio::steady_timer         m_Timer;

        // request from user
        struct RQ
        {
            ClientRequest request;
            Promise       promise;
        };
        std::list<RQ> m_WriteQueue;
        uint32_t      m_Serial = 1;

        // recv streams
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
            for (auto& sRQ : m_WriteQueue)
                sRQ.promise->set_exception(std::make_exception_ptr(std::runtime_error(m_FailReason)));
            m_WriteQueue.clear();
            for (auto& [sId, sPromise] : m_Streams)
                sPromise->set_exception(std::make_exception_ptr(std::runtime_error(m_FailReason)));
            m_Streams.clear();
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

        void send_one()
        {
            assert(!m_WriteQueue.empty());

            const uint32_t sStreamId = m_Serial;
            m_Serial += 2; // odd-numbered stream identifiers
            Header sHeader;
            sHeader.type   = Type::HEADERS;
            sHeader.flags  = Flags::END_HEADERS;
            sHeader.stream = sStreamId;
            auto& sPromise = m_Streams[sStreamId];

            auto sRQ = std::move(m_WriteQueue.front());
            m_WriteQueue.pop_front();
            sPromise = sRQ.promise;
            TRACE("create stream " << sStreamId << " for " << sRQ.request.url);

            if (sRQ.request.body.empty())
                sHeader.flags |= Flags::END_STREAM;
            m_Output.send(sHeader, m_Deflate(sRQ.request), m_WriteCoro.get());
            if (!sRQ.request.body.empty())
                m_Output.enqueue(sStreamId, std::move(sRQ.request.body));
        }

        //

        void async_i(RQ&& aRQ)
        {
            if (!m_FailReason.empty()) {
                aRQ.promise->set_exception(std::make_exception_ptr(std::runtime_error(m_FailReason)));
                return;
            }

            const bool sStart = m_WriteQueue.empty();
            m_WriteQueue.push_back(std::move(aRQ));
            if (sStart and !m_WriteCoro)
                spawn_write_coro();
        }

        void process_settings(const Frame& aFrame) override
        {
            if (aFrame.header.flags != 0) // filter out ACK_SETTINGS
                return;

            // ignore setting body

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

        void process_response(uint32_t aStreamId, Response&& aResponse) override
        {
            TRACE("got complete response");

            auto sIt = m_Streams.find(aStreamId);
            assert (sIt != m_Streams.end());
            sIt->second->set_value(std::move(aResponse));
            m_Streams.erase(sIt);
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
        }

        void hello()
        {
            const std::string_view sRequest("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
            m_Stream.expires_after(std::chrono::seconds(3));
            asio::async_write(m_Stream, asio::const_buffer(sRequest.data(), sRequest.size()), m_ReadCoro->yield[m_ReadCoro->ec]);
            if (m_ReadCoro->ec)
                throw m_ReadCoro->ec;

            Header sSettings;
            sSettings.type = Type::SETTINGS;
            m_Output.send(sSettings, {}, m_ReadCoro.get());
            TRACE("sent hello settings");

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
        , m_Timer(m_Service)
        , m_Input(m_Stream, this, false /* client */)
        , m_Output(m_Stream)
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
                p->async_i({std::move(aRequest), sPromise});
            });
            return sPromise->get_future();
        }

        void async(ClientRequest&& aRequest, Promise aPromise)
        {
            m_Strand.post([aRequest = std::move(aRequest), aPromise, p = shared_from_this()]() mutable {
                p->async_i({std::move(aRequest), aPromise});
            });
        }

    private:
        void read_coro(asio::yield_context yield)
        {
            try {
                m_ReadCoro = std::make_unique<CoroState>(CoroState{{}, yield});
                m_Input.assign(m_ReadCoro.get());
                connect();
                DEBUG("connected to " << m_Host << ':' << m_Port);
                hello();
                TRACE("http/2 negotiated");
                if (m_Notify)
                    m_Notify("connected");
                while (m_FailReason.empty())
                    m_Input.process_frame();
            } catch (const beast::error_code e) {
                fail(e);
            } catch (const std::exception& e) {
                fail(e);
            }
        }

        void spawn_write_coro()
        {
            asio::spawn(m_Strand, [this, p = shared_from_this()](asio::yield_context yield) mutable {
                try {
                    write_coro(yield);
                } catch (const beast::error_code e) {
                    fail(e);
                } catch (const std::exception& e) {
                    fail(e);
                }
            });
        }

        void write_coro(asio::yield_context yield)
        {
            TRACE("write out coro started");
            m_WriteCoro = std::make_unique<CoroState>(CoroState{{}, yield});
            m_Output.assign(m_WriteCoro.get());

            Util::Raii sCleanup([&]() {
                TRACE("write out coro finished");
                m_WriteCoro.reset();
                m_Output.assign(nullptr);
            });
            auto       sEnd = [this]() {
                return m_ReadCoro->ec or (m_WriteQueue.empty() and m_Output.idle());
            };
            while (!sEnd()) {
                m_Timer.expires_from_now(std::chrono::milliseconds(1));
                m_Timer.async_wait(m_WriteCoro->yield[m_WriteCoro->ec]);

                while (!m_WriteQueue.empty())
                    send_one();
                m_Output.flush();
            }
        }
    };

    struct Params
    {
        //unsigned max_connections  = 32;
        //time_t   delay            = 1;
    };

    class Manager : public std::enable_shared_from_this<Manager>, public Client
    {
        asio::io_service&        m_Service;
        asio::deadline_timer     m_Timer;
        asio::io_service::strand m_Strand;
        const Params             m_Params;

        // struct with host:port
        using Addr = Alive::Connection::Peer;

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

} // namespace asio_http::v2