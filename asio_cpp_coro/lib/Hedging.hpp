#pragma once

#include <stdint.h>

#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/steady_timer.hpp>

#include "API.hpp"

namespace AsioHttp {

    class Hedging
    {
        ClientPtr            m_Client;
        int                  m_Budget   = 0;
        static constexpr int INC_BUDGET = 1;
        static constexpr int DEC_BUDGET = 10;
        static constexpr int MAX_BUDGET = 100;

    public:
        Hedging(ClientPtr aClient)
        : m_Client(aClient)
        {
        }

        struct HRequest
        {
            Request                                        request;
            unsigned                                       timeout_ms;
            std::function<std::string()>                   peer;
            std::function<std::string(const std::string&)> hedge;
        };

        ba::awaitable<Response> perform(HRequest&& aRequest)
        {
            using namespace boost::asio::experimental::awaitable_operators;

            const std::string sPeer = aRequest.peer();

            Request sNormalRequest{
                .method  = aRequest.request.method,
                .url     = "http://" + sPeer + aRequest.request.url,
                .headers = aRequest.request.headers,
                .body    = aRequest.request.body,
            };

            if (m_Budget >= DEC_BUDGET) {
                ba::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                sTimer.expires_from_now(std::chrono::milliseconds(aRequest.timeout_ms));

                std::variant<Response, std::monostate> sResult =
                    co_await (m_Client->perform(std::move(sNormalRequest)) || sTimer.async_wait(ba::use_awaitable));
                if (sResult.index() == 0) {
                    m_Budget = std::min(MAX_BUDGET, m_Budget + INC_BUDGET);
                    co_return std::get<Response>(sResult);
                } else {
                    m_Budget = std::max(0, m_Budget - DEC_BUDGET);
                    Request sHedgeRequest{
                        .method  = aRequest.request.method,
                        .url     = "http://" + aRequest.hedge(sPeer) + aRequest.request.url,
                        .headers = std::move(aRequest.request.headers),
                        .body    = std::move(aRequest.request.body),
                    };
                    co_return co_await m_Client->perform(std::move(sHedgeRequest));
                }
            } else { // no budget for hedging
                m_Budget += INC_BUDGET;
                co_return co_await m_Client->perform(std::move(sNormalRequest));
            }
        }
    };

} // namespace AsioHttp