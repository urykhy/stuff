#pragma once

#include <list>
#include <mutex>
#include <string>

#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>

#include "API.hpp"

namespace asio_http {
    struct Router
    {
        using Handler = std::function<void(asio::io_service&, const Request&, Response&, asio::yield_context)>;

    private:
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        struct Entry
        {
            std::string path;
            bool        prefix = true;
            Handler     handler;
        };

        std::list<Entry> m_Locations;

        bool match(const Entry& aEntry, const boost::beast::string_view& aTarget) const
        {
            if (aEntry.prefix)
                return aTarget.starts_with(aEntry.path);
            else
                return aTarget == aEntry.path;
        }

    public:
        void insert(const std::string& aPath, const Handler aHandler, bool aPrefix = false)
        {
            Lock sLock(m_Mutex);
            m_Locations.push_back({aPath, aPrefix, aHandler});
        }

        void call(asio::io_service& aService, Request& aRequest, Response& aResponse, asio::yield_context yield) const
        {
            boost::beast::string_view sTarget = aRequest.target();

            size_t sQueryPos = sTarget.find('?');
            if (sQueryPos != boost::beast::string_view::npos)
                sTarget.remove_suffix(sTarget.size() - sQueryPos);

            Lock sLock(m_Mutex);
            for (auto& x : m_Locations)
                if (match(x, sTarget)) {
                    sLock.unlock();
                    try {
                        x.handler(aService, aRequest, aResponse, yield);
                    } catch (const std::exception& e) {
                        aResponse = {};
                        aResponse.result(http::status::internal_server_error);
                    }
                    return;
                }

            aResponse.result(http::status::not_found);
        }
    };

    using RouterPtr = std::shared_ptr<Router>;
} // namespace asio_http
