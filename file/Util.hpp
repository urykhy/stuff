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

    inline std::string get_extension(const std::string& aFilename)
    {
        size_t found = aFilename.find_last_of(".");
        return aFilename.substr(found + 1);
    }
} // namespace File