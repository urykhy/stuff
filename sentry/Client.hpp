#pragma once

#include <curl/Curl.hpp>
#include "Message.hpp"

namespace Sentry
{
    struct Client
    {
        struct Params {
            std::string url;
            std::string key;
            std::string secret;
            std::string client = "sentry++/1";
        };
    private:

        const Params m_Sentry;
        Curl::Client::Params m_Params;
        Curl::Client m_Client;
    public:

        Client(const Params& aSentry)
        : m_Sentry(aSentry)
        , m_Client(m_Params)
        {
            using Header = Curl::Client::Params::Header;
            m_Params.headers.emplace_back(Header{"Content-Type","application/json"});
            m_Params.headers.emplace_back(Header{"X-Sentry-Auth",std::string("Sentry ")
                + "sentry_key="    + m_Sentry.key + ", "
                + "sentry_secret=" + m_Sentry.secret + ", "
                + "sentry_client=" + m_Sentry.client + ", "
                + "sentry_version=7"
            });
        }

        Curl::Client::Result send(const Message& aMsg)
        {
            return m_Client.POST(m_Sentry.url, aMsg.to_string());
        }
    };

} // namespace Sentry