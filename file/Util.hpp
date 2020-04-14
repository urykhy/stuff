#pragma once

#include <string>

namespace File
{
    inline std::string get_filename(const std::string& str)
    {
        size_t found = str.find_last_of("/");
        return str.substr(found + 1);
    }

    inline std::string get_basename(const std::string& str)
    {
        size_t found = str.find_last_of("/");
        if (found == std::string::npos)
            return "";
        return str.substr(0, found);
    }

    inline std::string get_extension(const std::string& aFilename, bool aSkipTmp = true)
    {
        size_t found = aFilename.find_last_of(".");
        std::string sExt = aFilename.substr(found + 1);
        if (aSkipTmp and sExt.size() == 10 and 0 == sExt.compare(0, 4, "tmp-")) // is this File::Tmp extension ?
        {
            auto second = aFilename.rfind(".", found - 1);
            sExt = aFilename.substr(second + 1, found - second - 1);
        }
        return sExt;
    }
} // namespace File