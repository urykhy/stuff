#pragma once

#include <glob.h>

#include <filesystem>

#include <boost/algorithm/string/predicate.hpp>

#include <unsorted/Raii.hpp>

namespace File {
    bool isDirectory(const std::string& aPath)
    {
        namespace fs = std::filesystem;
        return fs::exists(aPath) && !fs::is_directory(aPath);
    }

    using FileList = std::vector<std::string>;
    size_t countFiles(const std::string& aPath)
    {
        namespace fs    = std::filesystem;
        uint64_t sCount = 0;

        const fs::directory_iterator sEnd;
        for (fs::directory_iterator sIter(aPath); sIter != sEnd; ++sIter)
            sCount++;

        return sCount;
    }

    FileList listFiles(const std::string& aPath, const std::string& aExt)
    {
        namespace fs = std::filesystem;
        FileList sResult;

        const fs::directory_iterator sEnd;
        for (fs::directory_iterator sIter(aPath); sIter != sEnd; ++sIter) {
            if (!fs::is_regular_file(sIter->status()))
                continue;
            const std::string sName = sIter->path().native();
            if (boost::algorithm::ends_with(sName, aExt))
                sResult.push_back(sName);
        }

        return sResult;
    }

    FileList glob(const std::string& aPattern)
    {
        FileList   sResult;
        glob_t     sGlob;
        Util::Raii sCleanup([&sGlob]() { globfree(&sGlob); });

        if (0 != ::glob(aPattern.c_str(), GLOB_ERR | GLOB_MARK | GLOB_TILDE_CHECK, nullptr, &sGlob))
            throw std::runtime_error("File::Glob failed");

        for (unsigned i = 0; i < sGlob.gl_pathc; i++)
            sResult.push_back(sGlob.gl_pathv[i]);

        return sResult;
    }
} // namespace File
