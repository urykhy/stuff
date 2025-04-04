#pragma once

#include <ranges>

#include "API.hpp"
#include "asio_http/API.hpp"
#include "asio_http/Asio.hpp"

#include <threads/Coro.hpp>
#include <unsorted/Log4cxx.hpp>

namespace KV {

    struct Params
    {
        std::string            url = "http://localhost:3080/kv";
        AsioHttp::ClientParams client{.alive = true};
        unsigned               linger_ms  = 5;
        unsigned               batch_size = 1000;
    };

    class Client
    {
        const Params        m_Params;
        AsioHttp::ClientPtr m_Client;

    public:
        Client(const Params& aParams = {})
        : m_Params(aParams)
        , m_Client(AsioHttp::createClient(m_Params.client))
        {
        }

        boost::asio::awaitable<Responses> Call(const Requests& aRequests)
        {
            cbor::omemstream sOutput;
            cbor::write(sOutput, aRequests);

            auto sRequest = AsioHttp::Request{.method = "POST", .url = m_Params.url, .body = std::move(sOutput.str())};
            auto sResult  = co_await m_Client->perform(std::move(sRequest));

            if (sResult.status != 200) {
                throw std::runtime_error("http response code " + std::to_string(sResult.status));
            }

            cbor::imemstream sInput(std::string_view(sResult.body));
            Responses        sResponses;
            cbor::read(sInput, sResponses);
            co_return sResponses;
        }
    };

    class BatchClient : public std::enable_shared_from_this<BatchClient>
    {
        const Params m_Params;
        Client       m_Client;
        uint64_t     m_HttpRequests = 0;

        struct Info
        {
            Request               request;
            Response              response;
            Threads::Coro::Waiter waiter;

            Info(const Request& aRequest)
            : request(aRequest)
            {
            }
        };
        using InfoPtr = std::shared_ptr<Info>;

        struct Batch
        {
            std::vector<InfoPtr>      info;
            boost::asio::steady_timer timer;

            Batch(boost::asio::any_io_executor aExecutor, unsigned aLingerMs, unsigned aMax)
            : timer(aExecutor)
            {
                timer.expires_from_now(std::chrono::milliseconds(aLingerMs));
                info.reserve(aMax);
            }
        };
        using BatchPtr = std::shared_ptr<Batch>;
        BatchPtr m_Queue;

        boost::asio::awaitable<void> Perform(BatchPtr aBatch)
        {
            Requests sRequests;
            sRequests.reserve(aBatch->info.size());
            for (const auto& x : aBatch->info) {
                sRequests.push_back(x->request);
            }
            m_HttpRequests++;
            Responses sResponses = co_await m_Client.Call(sRequests);
            for (auto [x, y] : std::views::zip(aBatch->info, sResponses)) {
                x->response = y;
                x->waiter.notify();
            }
        }

        boost::asio::awaitable<InfoPtr> Put(const Request& aRequest)
        {
            using namespace boost::asio;
            const auto sExecutor = co_await this_coro::executor;
            auto self = shared_from_this();

            auto sCreate = [&]() { self->m_Queue = std::make_shared<Batch>(sExecutor, self->m_Params.linger_ms, self->m_Params.batch_size); };

            auto sPut = [self, &aRequest]() -> auto {
                self->m_Queue->info.push_back(std::make_shared<Info>(aRequest));
                return self->m_Queue->info.back();
            };

            auto sSpawn = [self]() -> awaitable<void> {
                auto sBatch = self->m_Queue;
                co_spawn(
                    co_await this_coro::executor,
                    [self, sBatch]() -> awaitable<void> {
                        [[maybe_unused]] auto [sError] = co_await sBatch->timer.async_wait(as_tuple(use_awaitable));
                        if (self->m_Queue == sBatch) {
                            self->m_Queue.reset();
                        }
                        co_await self->Perform(sBatch);
                    },
                    detached); // FIXME: handle exceptions
            };

            InfoPtr sInfo;
            if (!m_Queue) {
                sCreate();
                sInfo = sPut();
                co_await sSpawn();
            } else if (m_Queue->info.size() >= m_Params.batch_size) {
                m_Queue->timer.cancel();
                sCreate();
                sInfo = sPut();
                co_await sSpawn();
            } else {
                sInfo = sPut();
            }
            co_return sInfo;
        }

    public:
        BatchClient(const Params& aParams = {})
        : m_Params(aParams)
        , m_Client(aParams)
        {
        }

        boost::asio::awaitable<Response> Call(Request& aRequest)
        {
            auto sInfo = co_await Put(aRequest);
            co_await sInfo->waiter.wait();
            co_return sInfo->response;
        }

        uint64_t HttpRequests() const {
            return m_HttpRequests;
        }
    };

} // namespace KV