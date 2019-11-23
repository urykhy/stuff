#pragma once

#include <regex>
#include <string>
#include <vector>

#include "Parser.hpp"

namespace Parse
{
    using StringList = std::vector<std::string>;

    StringList Autoindex(const std::string& aBody)
    {
        const std::regex sRegex("<a href=\"(.*)\">\\1</a>");
        StringList sResult;

        simple(aBody, [&](const boost::string_ref& aStr) mutable {
            std::smatch sMatch;
            const std::string sTmp(aStr.data(), aStr.size());
            if (std::regex_search(sTmp, sMatch, sRegex))
                sResult.push_back(sMatch[1]);
        }, '\n');

        return sResult;
    }
} // namespace Parser