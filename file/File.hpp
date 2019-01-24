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

}
