#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace asio_http
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    struct Router
    {
        using Handler = std::function<void(const Request&, Response&)>;

    private:
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
            m_Locations.push_back({aLoc, aHandler});
        }

        void call(const Request& aRequest, Response& aResponse) const
        {
            for (auto& [sLoc, sHandler]: m_Locations)
                if (match(sLoc, aRequest))
                {
                    sHandler(aRequest, aResponse);
                    return;
                }

            aResponse.result(http::status::not_found);
        }
    };

    using RouterPtr = std::shared_ptr<Router>;
}