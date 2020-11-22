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

    template <class T>
    void multipart(std::string_view aData, std::string_view aBoundary, T&& aHandler)
    {
        const std::string_view sDisposition = "Content-Disposition: form-data;";
        const std::string_view sType = "Content-Type: ";
        const std::string_view sDone = "--";

        struct Meta
        {
            std::string_view name;
            std::string_view filename;
            std::string_view content_type;
        };
        Meta sMeta;
        enum STATE
        {
            IDLE,
            HEADER,
            COLLECT,
            DONE
        };
        STATE            sState{};
        std::string_view sData;

        auto sParseDisposition = [&](std::string_view aLine) {
            simple(
                aLine.substr(sDisposition.size()), [&](auto aItem) {
                    String::trim(aItem);
                    auto sPos = aItem.find("=\"");
                    if (sPos == std::string_view::npos)
                        return;
                    std::string_view sName = aItem.substr(0, sPos);
                    if (sPos + 3 >= aItem.size())
                        return;
                    std::string_view sValue = aItem.substr(sPos + 2, aItem.size() - sPos - 3);
                    if (sName == "name")
                        sMeta.name = sValue;
                    else if (sName == "filename")
                        sMeta.filename = sValue;
                },
                ';');
        };

        simple(
            aData, [&](std::string_view aLine) {
                switch (sState) {
                case IDLE:
                    if (aLine == aBoundary)
                        sState = HEADER;
                    break;
                case HEADER:
                    if (aLine == "")
                        sState = COLLECT;
                    else if (String::starts_with(aLine, sDisposition))
                        sParseDisposition(aLine);
                    else if (String::starts_with(aLine, sType))
                        sMeta.content_type = aLine.substr(sType.size());
                    break;
                case COLLECT:
                    if (String::starts_with(aLine, aBoundary)) {
                        if (sData.data() == nullptr)
                            throw std::invalid_argument("unexpected boundary");
                        sData = std::string_view(sData.data(), aLine.data() - sData.data() - 1);
                        aHandler(sMeta, sData);
                        sState = HEADER;
                        sMeta  = {};
                        sData  = {};
                        if (aLine.substr(aBoundary.size()) == sDone)
                            sState = DONE;
                    } else {
                        if (sData.empty())
                            sData = aLine; // remember start pointer
                    }
                    break;
                case DONE:
                    break;
                }
            },
            '\n');
        if (sState == IDLE)
            throw std::invalid_argument("not multipart data");
        if (sState == COLLECT)
            throw std::invalid_argument("incomplete data");
        if (sState != DONE)
            throw std::invalid_argument("no termination marker");
    }

} // namespace Parser
