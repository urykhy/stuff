#pragma once

#ifndef ASIO_HTTP_LIBRARY_HEADER
#include "Output.hpp"
#include "Parser.hpp"
#endif

#include <unsorted/Raii.hpp>

namespace asio_http::v2 {
#ifdef ASIO_HTTP_LIBRARY_HEADER
    std::shared_ptr<Client> makeClient(asio::io_service& aService);
#else
    struct Peer : public std::enable_shared_from_this<Peer>, parser::API
    {
        using Notify = std::function<void(const std::string&)>;

    private:
        asio::io_service& m_Service;
        const std::string m_Host;
        const std::string m_Port;
        Strand            m_Strand;
        beast::tcp_stream m_Stream;
        uint32_t          m_Serial = 1;

        // pending requests
        struct PendingStream
        {
            ClientRequest request;
            Promise       promise;
        };
        std::list<PendingStream> m_Pending;

        // active streams
        std::map<uint32_t, Promise> m_Streams;

        InputBuf     m_Input;
        parser::Main m_Parser;
        Output       m_Output;
        Notify       m_Notify;
        std::string  m_FailReason;

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

        void connect(asio::yield_context yield)
        {
            beast::error_code ec;
            tcp::resolver     sResolver{m_Service};
            m_Stream.expires_after(std::chrono::milliseconds(100));

            auto const sAddr = sResolver.async_resolve(m_Host, m_Port, yield[ec]);
            if (ec)
                throw ec;
            m_Stream.async_connect(sAddr, yield[ec]);
            if (ec)
                throw ec;
            m_Stream.socket().set_option(tcp::no_delay(true));

            DEBUG("connected to " << m_Host << ':' << m_Port);
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

        // parser::API
        void established() override
        {
            if (m_Notify)
                m_Notify("connected");
        }
        parser::MessagePtr new_message(uint32_t aID) override
        {
            return std::make_shared<parser::AsioResponse>();
        }
        void process_message(uint32_t aID, parser::MessagePtr&& aMessage) override
        {
            TRACE("got complete response " << aID);
            auto* sResponse = static_cast<parser::AsioResponse*>(aMessage.get());

            auto sIt = m_Streams.find(aID);
            assert(sIt != m_Streams.end());
            sIt->second->set_value(std::move(sResponse->response));
            m_Streams.erase(sIt);

            if (!m_Pending.empty()) {
                auto& [sRequest, sPromise] = m_Pending.front();
                initiate(sRequest, sPromise);
                m_Pending.pop_front();
            }
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
        Peer(asio::io_service& aService, const std::string aHost, const std::string& aPort, Notify&& aNotify = {})
        : m_Service(aService)
        , m_Host(aHost)
        , m_Port(aPort)
        , m_Strand(m_Service.get_executor())
        , m_Stream(m_Service)
        , m_Parser(parser::CLIENT, this)
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
            asio::spawn(m_Strand, [p = shared_from_this()](asio::yield_context yield) mutable {
                p->read_coro(yield);
            });
        }

        std::future<Response> async(ClientRequest&& aRequest)
        {
            auto sPromise = std::make_shared<std::promise<Response>>();
            asio::post(m_Strand, [aRequest = std::move(aRequest), sPromise, p = shared_from_this()]() mutable {
                p->async_i(aRequest, sPromise);
            });
            return sPromise->get_future();
        }

        void async(ClientRequest&& aRequest, Promise aPromise)
        {
            asio::post(m_Strand, [aRequest = std::move(aRequest), aPromise, p = shared_from_this()]() mutable {
                p->async_i(aRequest, aPromise);
            });
        }

    private:
        void read_coro(asio::yield_context yield)
        {
            CATAPULT_THREAD("client")
            try {
                connect(yield);
                spawn_write_coro();

                // FIXME: read-loop ~ copy-paste from Server
                beast::error_code ec;
                std::string       sBuffer;
                while (m_FailReason.empty()) {
                    sBuffer.resize(0);
                    sBuffer.reserve(m_Parser.hint());
                    while (m_Parser.hint() > 0) {
                        if (m_Input.append(sBuffer, m_Parser.hint() - sBuffer.size()))
                            break;
                        m_Stream.expires_after(std::chrono::seconds(30));
                        // CATAPULT_EVENT("input", "async_read_some");
                        size_t sNew = m_Stream.async_read_some(m_Input.buffer(), yield[ec]);
                        if (ec)
                            throw ec;
                        m_Input.push(sNew);
                    }
                    assert(sBuffer.size() == m_Parser.hint());
                    m_Parser.process(sBuffer);
                }
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
        asio::io_service&    m_Service;
        Strand               m_Strand;
        asio::deadline_timer m_Timer;

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
        Manager(asio::io_service& aService)
        : m_Service(aService)
        , m_Strand(aService.get_executor())
        , m_Timer(m_Strand)
        {
        }

        void start_cleaner()
        {
            m_Timer.expires_from_now(boost::posix_time::milliseconds(1000));
            m_Timer.async_wait([this, p = this->shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    // TODO: close failed connections too (once params.delay passed)
                    TRACE("close idle connections ...");
                    start_cleaner();
                }
            });
        }

        std::future<Response> async(ClientRequest&& aRequest) override
        {
            auto sPromise = std::make_shared<std::promise<Response>>();
            asio::post(m_Strand, [aRequest = std::move(aRequest), sPromise, p = shared_from_this()]() mutable {
                p->async_i({std::move(aRequest), sPromise});
            });
            return sPromise->get_future();
        }

        std::future<Response> async_y(ClientRequest&& aRequest, asio::yield_context yield) override
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
                    [sWeakPtr, p = shared_from_this()](const std::string& aMsg) {
                        asio::post(p->m_Strand, [sWeakPtr, aMsg, p]() { p->notify_i(sWeakPtr, aMsg); });
                    });
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
        auto sClient = std::make_shared<Manager>(aService);
        sClient->start_cleaner();
        return sClient;
    }
#endif // ASIO_HTTP_LIBRARY_HEADER
} // namespace asio_http::v2
