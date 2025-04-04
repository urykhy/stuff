#include <list>

#include "API.hpp"
#include "Asio.hpp"

namespace AsioHttp {

    class BasicServer : public std::enable_shared_from_this<BasicServer>, public IServer
    {
        const ServerParams                         m_Params;
        std::list<std::pair<std::string, Handler>> m_Handlers;

        ba::awaitable<BeastResponse> handle(BeastRequest&& aRequest)
        {
            BeastResponse sResponse;

            try {
                bool sFound  = false;
                auto sPrefix = aRequest.target();

                for (const auto& sItem : m_Handlers) {
                    if (sPrefix.starts_with(sItem.first)) {
                        sResponse = co_await sItem.second(std::move(aRequest));
                        sFound    = true;
                        break;
                    }
                }
                if (!sFound) {
                    sResponse.result(bb::http::status::not_found);
                }
            } catch (const std::exception& sException) {
                sResponse = {};
                sResponse.result(bb::http::status::internal_server_error);
            }

            co_return sResponse;
        }

        ba::awaitable<void> session(bb::tcp_stream& aStream)
        {
            bs::error_code  sError;
            bb::flat_buffer sBuffer;
            sBuffer.reserve(1024*64);

            auto sCheckError = [&](const char* aMsg) {
                if (sError)
                    throw std::runtime_error(aMsg + sError.message());
            };

            aStream.socket().set_option(tcp::no_delay(true));

            while (true) {
                aStream.expires_after(std::chrono::milliseconds(m_Params.idle_timeout));
                BeastRequest sRequest;
                co_await http::async_read(aStream, sBuffer, sRequest, ba::redirect_error(ba::use_awaitable, sError));
                sCheckError("read http request: ");

                BeastResponse sResponse = co_await handle(std::move(sRequest));

                sResponse.prepare_payload();
                if (0 == sResponse.count(http::field::server))
                    sResponse.set(http::field::server, "Beast/cxx");

                co_await http::async_write(aStream, sResponse, ba::redirect_error(ba::use_awaitable, sError));
                sCheckError("send http response: ");
                if (sResponse.need_eof()) // we should close the connection, the response indicated the "Connection: close"
                    break;

                sBuffer.consume(sBuffer.size()); // cleanup buffer
            }
            aStream.socket().shutdown(tcp::socket::shutdown_send, sError);
        }

    public:
        BasicServer(const ServerParams& aParams)
        : m_Params(aParams)
        {
        }

        void addHandler(const std::string& aPrefix, Handler aHandler) override
        {
            m_Handlers.push_back({aPrefix, std::move(aHandler)});
        }

        ba::awaitable<void> run() override
        {
            bs::error_code sError;
            auto           sExecutor = co_await this_coro::executor;

            auto const    sAddress  = ba::ip::make_address("0.0.0.0");
            auto const    sEndpoint = tcp::endpoint(sAddress, m_Params.port);
            tcp::acceptor sAcceptor(sExecutor);
            sAcceptor.open(sEndpoint.protocol());
            sAcceptor.set_option(ba::socket_base::reuse_address(true));
            sAcceptor.bind(sEndpoint, sError);
            if (sError) {
                throw std::runtime_error("Server: fail to bind: " + sError.message());
            }
            sAcceptor.listen();

            while (true) {
                auto sSocket = co_await sAcceptor.async_accept(ba::redirect_error(ba::use_awaitable, sError));
                if (sError) {
                    throw std::runtime_error("Server: fail to accept: " + sError.message());
                }
                ba::co_spawn(
                    sExecutor, [sSelf = shared_from_this(), sStream = bb::tcp_stream(std::move(sSocket))]() mutable -> ba::awaitable<void> {
                        co_return co_await sSelf->session(sStream);
                    },
                    ba::detached);
            }
        }
        virtual ~BasicServer() = default;
    };

    ServerPtr createServer(const ServerParams& aParams)
    {
        return std::make_shared<BasicServer>(aParams);
    }
} // namespace AsioHttp
