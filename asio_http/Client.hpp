#pragma once

// based on https://www.boost.org/doc/libs/master/libs/beast/example/http/client/coro/http_client_coro.cpp

#include <boost/asio/spawn.hpp>

#include <parser/Url.hpp>
#include <threads/Asio.hpp>

#include "Router.hpp"

namespace asio_http {
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
    using tcp       = boost::asio::ip::tcp;

    struct ClientRequest
    {
        using Header  = std::pair<http::field, std::string>;
        using Headers = std::list<Header>;

        http::verb  method = http::verb::get;
        std::string url;
        std::string body;
        Headers     headers;
        time_t      connect = 100; // timeout in ms
        time_t      total   = 3000;
    };

    using Promise = std::shared_ptr<std::promise<Response>>;

    inline void simple_client_session(asio::io_service& aService, ClientRequest&& aRequest, Promise aPromise, net::yield_context yield)
    {
        beast::error_code ec;
        tcp::resolver     sResolver{aService};
        beast::tcp_stream sStream{aService};

        auto    sParsed = Parser::url(aRequest.url);
        Request sInternal{aRequest.method, sParsed.query, 10}; // 10 is 1.0 http version
        sInternal.body() = std::move(aRequest.body);
        for (auto& [sField, sValue] : aRequest.headers)
            sInternal.set(sField, std::move(sValue));

        auto sReport = [&](const char* aMsg) {
            aPromise->set_exception(std::make_exception_ptr(std::runtime_error(aMsg + ec.message())));
        };

        sStream.expires_after(std::chrono::milliseconds(aRequest.connect));
        auto const sAddr = sResolver.async_resolve(sParsed.host, sParsed.port, yield[ec]);
        if (ec) {
            sReport("resolve: ");
            return;
        }

        sStream.async_connect(sAddr, yield[ec]);
        if (ec) {
            sReport("connect: ");
            return;
        }

        sStream.expires_after(std::chrono::milliseconds(aRequest.total));
        if (!sInternal.body().empty())
            sInternal.prepare_payload();
        http::async_write(sStream, sInternal, yield[ec]);
        if (ec) {
            sReport("write: ");
            return;
        }

        beast::flat_buffer sBuffer;
        Response           sResponse;

        http::async_read(sStream, sBuffer, sResponse, yield[ec]);
        if (ec) {
            sReport("read: ");
            return;
        }

        aPromise->set_value(std::move(sResponse));
        sStream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    inline std::future<Response> async(asio::io_service& aService, ClientRequest&& aRequest)
    {
        auto sPromise = std::make_shared<std::promise<Response>>();
        boost::asio::spawn(aService, [aService = std::ref(aService), aRequest = std::move(aRequest), sPromise](boost::asio::yield_context yield) mutable {
            simple_client_session(aService, std::move(aRequest), sPromise, yield);
        });
        return sPromise->get_future();
    }

    inline std::future<Response> async(Threads::Asio& aContext, ClientRequest&& aRequest)
    {
        return async(aContext.service(), std::move(aRequest));
    }

    inline std::future<Response> async(asio::io_service& aService, ClientRequest&& aRequest, boost::asio::yield_context yield)
    {
        auto sPromise = std::make_shared<std::promise<asio_http::Response>>();
        asio_http::simple_client_session(aService, std::move(aRequest), sPromise, yield);
        return sPromise->get_future();
    }
} // namespace asio_http
