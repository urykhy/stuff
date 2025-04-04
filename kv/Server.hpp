#pragma once

#include <unordered_map>

#include "API.hpp"
#include "asio_http/API.hpp"
#include "asio_http/Asio.hpp"

namespace KV {
    class Server : public std::enable_shared_from_this<Server>
    {
        std::unordered_map<std::string, std::string> m_Data;

        boost::asio::awaitable<Response> Get(Request& aRequest)
        {
            Response sResponse;
            sResponse.key = aRequest.key;
            if (auto x = m_Data.find(aRequest.key); x != m_Data.end()) {
                sResponse.value = x->second;
            }
            co_return sResponse;
        }

        boost::asio::awaitable<Response> Set(Request& aRequest)
        {
            Response sResponse;
            sResponse.key        = aRequest.key;
            m_Data[aRequest.key] = std::move(aRequest.value.value());
            co_return sResponse;
        }

    public:
        void Configure(AsioHttp::ServerPtr aServer)
        {
            using namespace AsioHttp;
            auto self = shared_from_this();
            aServer->addHandler("/kv", [self](BeastRequest&& aRequest) -> ba::awaitable<BeastResponse> {
                BeastResponse sResponse;
                if (aRequest.method() == http::verb::post) {
                    KV::Responses    sResponses;
                    KV::Requests     sRequests;
                    cbor::imemstream sInput(std::string_view(aRequest.body()));
                    cbor::read(sInput, sRequests);
                    for (auto& x : sRequests) {
                        if (x.value) {
                            sResponses.push_back(co_await self->Set(x));
                        } else {
                            sResponses.push_back(co_await self->Get(x));
                        }
                    }
                    cbor::omemstream sOutput;
                    cbor::write(sOutput, sResponses);
                    sResponse.body() = std::move(sOutput.str());
                    sResponse.result(http::status::ok);
                } else {
                    sResponse.result(http::status::method_not_allowed);
                }
                co_return sResponse;
            });
        }
    };
} // namespace KV