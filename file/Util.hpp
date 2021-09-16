#pragma once

#include <filesystem>
#include <string>

namespace File {
    inline std::string getFilename(const std::string& str)
    {
        size_t found = str.find_last_of("/");
        return str.substr(found + 1);
    }

    inline std::string getBasename(const std::string& str)
    {
        size_t found = str.find_last_of("/");
        if (found == std::string::npos)
            return "";
        return str.substr(0, found);
    }

    inline std::string getExtension(const std::string& aFilename, bool aSkipTmp = true)
    {
        size_t found = aFilename.find_last_of(".");
        if (found == std::string::npos)
            return "";
        std::string sExt = aFilename.substr(found + 1);
        if (aSkipTmp and sExt.size() >= 3 and 0 == sExt.compare(0, 3, "tmp")) // tmp extension ?
        {
            auto second = aFilename.rfind(".", found - 1);
            if (second == std::string::npos)
                return "";
            sExt = aFilename.substr(second + 1, found - second - 1);
        }
        return sExt;
    }

    inline std::string tmpName(const std::string& aPath, const std::string& sExt = ".tmp")
    {
        namespace fs = std::filesystem;
        fs::path sPath(aPath);
        return fs::path(sPath.parent_path() / ("." + sPath.filename().string() + sExt));
    }
} // namespace File
