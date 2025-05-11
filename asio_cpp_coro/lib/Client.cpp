#include <iostream>

#include "API.hpp"
#include "Asio.hpp"

#include <container/Pool.hpp>
#include <parser/Url.hpp>
#include <unsorted/Log4cxx.hpp>

namespace AsioHttp {

    static log4cxx::LoggerPtr sLogger = Logger::Get("http");

    static BeastRequest convert(Request&& aRequest, const std::string& aPath)
    {
        BeastRequest sInternal{http::string_to_verb(aRequest.method), aPath, 10}; // 10 is 1.0 http version
        sInternal.body() = std::move(aRequest.body);
        if (!sInternal.body().empty())
            sInternal.prepare_payload();
        for (auto& [sName, sValue] : aRequest.headers)
            sInternal.set(sName, std::move(sValue));
        return sInternal;
    }

    static Response convert(BeastResponse&& aResponse)
    {
        Response sResponse;
        sResponse.status = aResponse.result_int();
        sResponse.body   = aResponse.body();
        for (const auto& sHeader : aResponse.base()) {
            const auto sName                                = sHeader.name_string();
            const auto sValue                               = sHeader.value();
            sResponse.headers[{sName.data(), sName.size()}] = std::string{sValue.data(), sValue.size()};
        }
        return sResponse;
    }

    class BasicClient : public IClient
    {
        const ClientParams m_Params;

    public:
        BasicClient(const ClientParams& aParams)
        : m_Params(aParams)
        {
        }

        ba::awaitable<Response> perform(Request&& aRequest) override
        {
            bs::error_code sError;
            auto           sExecutor = co_await this_coro::executor;

            tcp::resolver  sResolver{sExecutor};
            bb::tcp_stream sStream{sExecutor};

            const auto sParsed   = Parser::url(aRequest.url);
            auto       sInternal = convert(std::move(aRequest), sParsed.path);

            auto sCheckError = [&](const char* aMsg) {
                if (sError) {
                    WARN("Client: fail to " << aMsg << ": "<< sError.message());
                    throw std::runtime_error(aMsg + sError.message());
                }
            };

            DEBUG("new connection " << &sStream << " to " << sParsed.host << ":" << sParsed.port);

            sStream.expires_after(std::chrono::milliseconds(m_Params.connect_timeout));
            auto const sAddr = co_await sResolver.async_resolve(sParsed.host, sParsed.port, ba::redirect_error(ba::use_awaitable, sError));
            sCheckError("resolve: ");

            co_await sStream.async_connect(sAddr, ba::redirect_error(ba::use_awaitable, sError));
            sCheckError("connect: ");
            sStream.socket().set_option(tcp::no_delay(true));

            sStream.expires_after(std::chrono::milliseconds(m_Params.total_timeout));
            co_await http::async_write(sStream, sInternal, ba::redirect_error(ba::use_awaitable, sError));
            sCheckError("send http request: ");

            bb::flat_buffer sBuffer;
            sBuffer.reserve(1024*64);
            BeastResponse   sResponse;
            co_await http::async_read(sStream, sBuffer, sResponse, ba::redirect_error(ba::use_awaitable, sError));
            sCheckError("read http response: ");

            sStream.socket().shutdown(tcp::socket::shutdown_both, sError);
            co_return convert(std::move(sResponse));
        }
    };

    class AliveClient : public IClient
    {
        const ClientParams m_Params;

        struct Key
        {
            std::string host;
            std::string port;
            bool        operator<(const Key& aOther) const
            {
                return std::tie(host, port) < std::tie(aOther.host, aOther.port);
            }
        };
        struct Deleter
        {
            void operator()(bb::tcp_stream* aPtr) const
            {
                bs::error_code sError;
                aPtr->socket().shutdown(tcp::socket::shutdown_both, sError);
                delete aPtr;
            }
        };
        using ConnectionPtr = std::shared_ptr<bb::tcp_stream>;
        Container::KeyPool<Key, ConnectionPtr> m_Pool;

        time_t m_ClenerLastTime = 0;

    public:
        AliveClient(const ClientParams& aParams)
        : m_Params(aParams)
        {
        }

        ba::awaitable<Response> perform(Request&& aRequest) override
        {
            bs::error_code sError;
            auto           sExecutor = co_await this_coro::executor;

            time_t sNow = ::time(nullptr);
            if (m_ClenerLastTime < sNow) {
                m_Pool.cleanup();
                m_ClenerLastTime = sNow;
            }

            const auto sParsed   = Parser::url(aRequest.url);
            auto       sInternal = convert(std::move(aRequest), sParsed.path);

            auto sCheckError = [&](const char* aMsg) {
                if (sError)
                    throw std::runtime_error(aMsg + sError.message());
            };

            std::shared_ptr<bb::tcp_stream> sStream;

            const Key sKey = {.host = sParsed.host, .port = sParsed.port};
            if (auto sCached = m_Pool.get(sKey); sCached.has_value()) {
                sStream = sCached.value();
                DEBUG("reuse connection " << sStream.get() << " to " << sParsed.host << ":" << sParsed.port);
            } else {
                tcp::resolver sResolver{sExecutor};
                sStream = ConnectionPtr(new bb::tcp_stream(sExecutor), Deleter());
                DEBUG("new connection " << sStream.get() << " to " << sParsed.host << ":" << sParsed.port);

                sStream->expires_after(std::chrono::milliseconds(m_Params.connect_timeout));
                auto const sAddr = co_await sResolver.async_resolve(sParsed.host, sParsed.port, ba::redirect_error(ba::use_awaitable, sError));
                sCheckError("resolve: ");

                co_await sStream->async_connect(sAddr, ba::redirect_error(ba::use_awaitable, sError));
                sCheckError("connect: ");
                sStream->socket().set_option(tcp::no_delay(true));
            }

            sStream->expires_after(std::chrono::milliseconds(m_Params.total_timeout));
            co_await http::async_write(*sStream, sInternal, ba::redirect_error(ba::use_awaitable, sError));
            sCheckError("send http request: ");

            bb::flat_buffer sBuffer;
            sBuffer.reserve(1024*64);
            BeastResponse   sResponse;
            co_await http::async_read(*sStream, sBuffer, sResponse, ba::redirect_error(ba::use_awaitable, sError));
            sCheckError("read http response: ");

            DEBUG("keep alive connection " << sStream.get() << " to " << sParsed.host << ":" << sParsed.port);
            m_Pool.insert(sKey, sStream);
            co_return convert(std::move(sResponse));
        }
    };

    ClientPtr createClient(const ClientParams& aParams)
    {
        if (!aParams.alive)
            return std::make_shared<BasicClient>(aParams);
        else
            return std::make_shared<AliveClient>(aParams);
    }
} // namespace AsioHttp
