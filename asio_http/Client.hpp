#pragma once

// based on https://www.boost.org/doc/libs/master/libs/beast/example/http/client/coro/http_client_coro.cpp

#include <threads/Asio.hpp>
#include "Router.hpp"

namespace asio_http
{
    struct ClientRequest
    {
        std::string host;
        std::string port;
        Request     request;

        time_t      connect = 100;  // timeout in ms
        time_t      total   = 3000;
    };

    using Promise = std::shared_ptr<std::promise<Response>>;

    inline void simple_client_session(asio::io_service& aService, ClientRequest&& aRequest, Promise aPromise, net::yield_context yield)
    {
        beast::error_code ec;
        tcp::resolver     sResolver{aService};
        beast::tcp_stream sStream{aService};

        auto sReport = [&](const char* aMsg) {
            aPromise->set_exception(std::make_exception_ptr(std::runtime_error(aMsg + ec.message())));
        };

        auto const sAddr = sResolver.async_resolve(aRequest.host, aRequest.port, yield[ec]);
        if (ec) { sReport("resolve: "); return; }

        sStream.expires_after(std::chrono::milliseconds(aRequest.connect));
        sStream.async_connect(sAddr, yield[ec]);
        if (ec) { sReport("connect: "); return; }

        sStream.expires_after(std::chrono::milliseconds(aRequest.total));
        http::async_write(sStream, aRequest.request, yield[ec]);
        if (ec) { sReport("write: "); return; }

        boost::beast::flat_buffer sBuffer;
        Response sResponse;

        http::async_read(sStream, sBuffer, sResponse, yield[ec]);
        if (ec) { sReport("read: "); return; }

        aPromise->set_value(std::move(sResponse));
        sStream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    std::future<Response> async(Threads::Asio& aContext, ClientRequest&& aRequest)
    {
        auto sPromise = std::make_shared<std::promise<Response>>();
        boost::asio::spawn(aContext.service(), [aService = std::ref(aContext.service()), aRequest = std::move(aRequest), sPromise](boost::asio::yield_context yield) mutable {
            simple_client_session(aService, std::move(aRequest), sPromise, yield);
        });
        return sPromise->get_future();
    }
}