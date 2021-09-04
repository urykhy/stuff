#pragma once

#include <stdexcept>
#include <string>

#include "Hex.hpp"
#include "Parser.hpp"

#include <string/String.hpp>

namespace Parser {
    inline std::string url_decode(std::string_view aData)
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
                    aHandler(sKey, url_decode(sValue));
                } else
                    aHandler(aStr, std::string_view());
            },
            '&');
    }

    template <class T>
    void http_header_kv(std::string_view aHeader, T&& aHandler)
    {
        simple(
            aHeader, [aHandler = std::move(aHandler)](auto&& aStr) {
                auto sEnd = aStr.find('=');
                if (sEnd != std::string_view::npos) {
                    auto sKey   = aStr.substr(0, sEnd);
                    auto sValue = aStr.substr(sEnd + 1);
                    aHandler(sKey, sValue);
                } else
                    aHandler(aStr, std::string_view());
            },
            ',');
    }

    template <class T>
    void http_path_params(std::string_view aTarget, std::string_view aPattern, T&& aHandler)
    {
        size_t sPos = aPattern.find('{');
        if (sPos == std::string_view::npos) {
            return;
        }
        aTarget.remove_prefix(sPos);
        aPattern.remove_prefix(sPos);

        sPos = aTarget.find('?');
        if (sPos != std::string_view::npos)
            aTarget = aTarget.substr(0, sPos);

        std::vector<std::string_view> sNames;
        Parser::simple(aPattern, sNames, '/');

        std::vector<std::string_view> sValues;
        Parser::simple(aTarget, sValues, '/');

        if (sNames.size() != sValues.size())
            throw std::invalid_argument("expect " + std::to_string(sNames.size()) + " parameters, but got " + std::to_string(sValues.size()));

        for (size_t i = 0; i < sNames.size(); i++) {
            auto sName = sNames[i];
            if (sName.empty())
                continue;

            auto sLeft  = sName.find('{');
            auto sRight = sName.find('}');
            if (sLeft == std::string_view::npos or sRight == std::string_view::npos)
                continue;

            auto sValue = sValues[i];
            sValue.remove_prefix(sLeft);
            sValue.remove_suffix(sName.size() - sRight - 1);

            sName.remove_prefix(sLeft + 1);
            sName.remove_suffix(sName.size() + sLeft - sRight + 1);

            aHandler(sName, url_decode(sValue));
        }
    }
} // namespace Parser
