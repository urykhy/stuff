#pragma once
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>

namespace String
{
    // https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
    // trim from start (in place)
    inline void ltrim(std::string &s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
        {
            return !std::isspace(ch);
        }));
    }

    // trim from end (in place)
    inline void rtrim(std::string &s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
        {
            return !std::isspace(ch);
        }).base(), s.end());
    }

    // trim from both ends (in place)
    inline void trim(std::string &s)
    {
        ltrim(s);
        rtrim(s);
    }

    inline bool start_with(const std::string& s, const std::string& pa)
    {
        return s.compare(0, pa.size(), pa) == 0;
    }
}
