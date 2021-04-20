#pragma once

#include <string_view>

#include "Parser.hpp"

#include <string/String.hpp>

namespace Parser {
    template <class T>
    void multipart(std::string_view aData, std::string_view aBoundary, T&& aHandler)
    {
        const std::string_view sDisposition = "Content-Disposition: form-data;";
        const std::string_view sType        = "Content-Type: ";
        const std::string_view sDone        = "--";

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
                    throw std::invalid_argument("unexpected data after end marker");
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
