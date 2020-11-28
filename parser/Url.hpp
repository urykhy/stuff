#pragma once

#include <stdexcept>
#include <string>

#include <string/String.hpp>

#include "Hex.hpp"
#include "Parser.hpp"

namespace Parser {
    inline std::string url_decode(const std::string& aData)
    {
        enum
        {
            STAGE_INITIAL = 0,
            STAGE_FIRST,
            STAGE_SECOND
        };

        std::string sResult;
        sResult.reserve(aData.size());

        uint8_t stage  = 0;
        uint8_t decode = 0;

        for (const uint8_t i : aData) {
            switch (stage) {
            case STAGE_INITIAL:
                if (i != '%')
                    sResult.push_back(i);
                else
                    stage = STAGE_FIRST;
                break;
            case STAGE_FIRST:
                decode = (aux::restore(i) << 4);
                stage  = STAGE_SECOND;
                break;
            case STAGE_SECOND:
                decode += aux::restore(i);
                sResult.push_back(decode);
                decode = 0;
                stage  = STAGE_INITIAL;
                break;
            }
        }

        return sResult;
    }

    // get hostname, port, query from url
    inline auto url(const std::string& aUrl)
    {
        struct Parsed
        {
            std::string host;
            std::string port  = "80";
            std::string query = "/";
        };
        Parsed sParsed;

        if (!String::starts_with(aUrl, "http://"))
            throw std::invalid_argument("url must start with http://");

        enum State
        {
            HOST,
            PORT,
            QUERY
        };
        unsigned sState = HOST;

        for (size_t sPos = 7; sPos < aUrl.size(); sPos++) {
            char c = aUrl[sPos];

            switch (sState) {
            case HOST:
                switch (c) {
                case ':':
                    sState++;
                    sParsed.port.clear();
                    break;
                case '/': sState = QUERY; break;
                default: sParsed.host.push_back(c);
                }
                break;
            case PORT:
                switch (c) {
                case '/': sState++; break;
                default: sParsed.port.push_back(c);
                }
                break;
            case QUERY:
                sParsed.query.push_back(c);
                break;
            }
        }

        return sParsed;
    }

    template <class T>
    void http_query(std::string_view aQuery, T&& aHandler)
    {
        auto sBegin = aQuery.find('?');
        if (sBegin == std::string_view::npos)
            return;

        simple(
            aQuery.substr(sBegin + 1), [aHandler = std::move(aHandler)](auto&& aStr) {
                auto sEnd = aStr.find('=');
                if (sEnd != std::string_view::npos) {
                    auto sKey   = aStr.substr(0, sEnd);
                    auto sValue = aStr.substr(sEnd + 1);
                    aHandler(sKey, sValue);
                } else
                    aHandler(aStr, std::string_view());
            },
            '&');
    }
} // namespace Parser
