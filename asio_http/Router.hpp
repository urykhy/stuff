#pragma once

#include <list>
#include <mutex>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace asio_http {
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
    using tcp       = boost::asio::ip::tcp;

    using Request  = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    struct Router
    {
        using Handler = std::function<void(asio::io_service&, const Request&, Response&, asio::yield_context)>;

    private:
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        std::list<std::pair<std::string, Handler>> m_Locations;

        bool match(const std::string& aLoc, const Request& aReq) const
        {
            if (0 != aReq.target().compare(0, aLoc.size(), aLoc))
                return false;
            return true;
        }

    public:
        void insert(const std::string& aLoc, const Handler aHandler)
        {
            Lock sLock(m_Mutex);
            m_Locations.push_back({aLoc, aHandler});
        }

        void call(asio::io_service& aService, Request& aRequest, Response& aResponse, asio::yield_context yield) const
        {
            Lock sLock(m_Mutex);
            for (auto& [sLoc, sHandler] : m_Locations)
                if (match(sLoc, aRequest)) {
                    sLock.unlock();
                    sHandler(aService, aRequest, aResponse, yield);
                    return;
                }

            aResponse.result(http::status::not_found);
        }
    };

    using RouterPtr = std::shared_ptr<Router>;
} // namespace asio_http
