#pragma once

#include <memory>

#include "Router.hpp"
#include "Types.hpp"

#include <unsorted/Log4cxx.hpp>

namespace asio_http::v2 {

    class Peer : public std::enable_shared_from_this<Peer>
    {
        asio::io_service&        m_Service;
        const std::string        m_Host;
        const std::string        m_Port;
        asio::io_service::strand m_Strand;
        beast::tcp_stream        m_Stream;
        HPack                    m_Pack;

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
        struct Stream
        {
            Response response;
            Promise  promise;
            bool     m_NoBody = false;
        };
        std::map<uint32_t, Stream> m_Streams;

        Input  m_Input;
        Output m_Output;

        void send(RQ& aRQ)
        {
            const uint32_t sStreamId = m_Serial;
            m_Serial += 2; // odd-numbered stream identifiers

            TRACE("create stream " << sStreamId << " for " << aRQ.request.url);

            Header sHeader;
            sHeader.type   = Type::HEADERS;
            sHeader.flags  = Flags::END_HEADERS;
            sHeader.stream = sStreamId;

            auto& sStream   = m_Streams[sStreamId];
            sStream.promise = aRQ.promise;

            if (aRQ.request.body.empty())
                sHeader.flags |= Flags::END_STREAM;
            m_Output.send(sHeader, m_Pack.deflate(aRQ.request), m_WriteCoro.get());
            if (!aRQ.request.body.empty())
                m_Output.enqueue(sStreamId, std::move(aRQ.request.body));
        }

        void flush()
        {
            while (!m_WriteQueue.empty()) {
                send(m_WriteQueue.front());
                m_WriteQueue.pop_front();
            }
            m_Output.flush();
        }

        //

        void async_i(RQ&& aRQ)
        {
            const bool sStart = m_WriteQueue.empty();
            m_WriteQueue.push_back(std::move(aRQ));
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
                TRACE("got complete response");
                auto& sStream   = sIt->second;
                auto& sResponse = sStream.response;
                sResponse.body().assign(m_Input.extract(sStreamId));
                sStream.promise->set_value(std::move(sResponse));
                m_Streams.erase(sIt);
            }
        }

        void process_headers(const Input::Frame& aFrame)
        {
            const uint32_t sStreamId = aFrame.header.stream;
            assert(sStreamId != 0);

            auto sIt = m_Streams.find(sStreamId);
            if (sIt == m_Streams.end())
                throw std::logic_error("nx response stream");
            auto& sStream   = sIt->second;
            auto& sResponse = sStream.response;

            m_Pack.inflate(aFrame.header, aFrame.body, sResponse);

            if (aFrame.header.flags & Flags::END_STREAM)
                sStream.m_NoBody = true;
            if (aFrame.header.flags & Flags::END_HEADERS and sStream.m_NoBody) {
                TRACE("got complete response");
                sStream.promise->set_value(std::move(sResponse));
                m_Streams.erase(sIt);
            }
        }

        void process_settings(const Input::Frame& aFrame)
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

        void process_window_update(const Input::Frame& aFrame)
        {
            m_Output.window_update(aFrame.header, aFrame.body);
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

        bool process_frame()
        {
            m_Stream.expires_after(std::chrono::seconds(30));
            auto sFrame = m_Input.recv();

            switch (sFrame.header.type) {
            case Type::DATA: process_data(sFrame); break;
            case Type::HEADERS: process_headers(sFrame); break;
            case Type::SETTINGS: process_settings(sFrame); break;
            case Type::WINDOW_UPDATE: process_window_update(sFrame); break;
            case Type::CONTINUATION: process_headers(sFrame); break;
            default: break;
            }

            return true;
        }

    public:
        Peer(asio::io_service& aService, const std::string aHost, const std::string& aPort)
        : m_Service(aService)
        , m_Host(aHost)
        , m_Port(aPort)
        , m_Strand(m_Service)
        , m_Stream(m_Service)
        , m_Timer(m_Service)
        , m_Input(m_Stream)
        , m_Output(m_Stream)
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
                while (true) {
                    if (!process_frame())
                        break;
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

} // namespace asio_http::v2