#pragma once

#include <string>
#include <fstream>

namespace File
{
    inline std::string to_string(const std::string& aFilename)
    {
        std::ifstream sFile(aFilename);
        sFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        std::string sBuf((std::istreambuf_iterator<char>(sFile)), std::istreambuf_iterator<char>());
        return sBuf;
    }

    template<class F>
    void by_string(const std::string& aFilename, F aFunc)
    {
        std::ifstream sFile(aFilename);
        sFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        std::string sBuf;

        try
        {
            while (std::getline(sFile, sBuf))
                aFunc(sBuf);
        } catch (...) {
            if (!sFile.eof())
                throw;
        }
    }

    inline std::string get_filename(const std::string& str)
    {
        size_t found = str.find_last_of("/");
        return str.substr(found + 1);
    }

    inline std::string get_basename(const std::string& str)
    {
        size_t found = str.find_last_of("/");
        return str.substr(0, found);
    }
}
