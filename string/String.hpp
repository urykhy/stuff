#pragma once
#include <algorithm>
#include <cctype>
#include <locale>
#include <string>
#include <string_view>

namespace String {
    // https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
    // trim from start (in place)
    inline void ltrim(std::string& s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                    return !std::isspace(ch);
                }));
    }

    // trim from end (in place)
    inline void rtrim(std::string& s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
                    return !std::isspace(ch);
                }).base(),
                s.end());
    }

    // trim from both ends (in place)
    inline void trim(std::string& s)
    {
        ltrim(s);
        rtrim(s);
    }

    inline void ltrim(std::string_view& s)
    {
        while (!s.empty() and std::isspace(s.front()))
            s.remove_prefix(1);
    }

    inline void rtrim(std::string_view& s)
    {
        while (!s.empty() and std::isspace(s.back()))
            s.remove_suffix(1);
    }

    inline void trim(std::string_view& s)
    {
        ltrim(s);
        rtrim(s);
    }

    inline bool starts_with(const std::string_view& s, const std::string_view& pa)
    {
        return s.compare(0, pa.size(), pa) == 0;
    }

    inline bool ends_with(const std::string_view& s, const std::string_view& pa)
    {
        if (s.size() < pa.size())
            return false;
        return s.compare(s.size() - pa.size(), pa.size(), pa) == 0;
    }

    std::string replace(const std::string& aSrc, std::string_view aFrom, std::string aTo)
    {
        std::string sResult = aSrc;
        const auto sPos = aSrc.find(aFrom);
        if (sPos != std::string::npos)
            sResult.replace(sPos, aFrom.size(), aTo);
        return sResult;
    }
} // namespace String
