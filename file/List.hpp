#pragma once

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

namespace File
{
    bool is_directory(const std::string& aPath)
    {
        namespace fs = boost::filesystem;
        return fs::exists(aPath) && !fs::is_directory(aPath);
    }

    using FileList = std::vector<std::string>;

    FileList ReadDir(const std::string& aPath, const std::string& aExt)
    {
        namespace fs = boost::filesystem;
        FileList sResult;

        const fs::directory_iterator sEnd;
        for (fs::directory_iterator sIter(aPath) ; sIter != sEnd ; ++sIter)
        {
            if (!fs::is_regular_file(sIter->status()))
                continue;
            const std::string sName = sIter->path().native();
            if (boost::algorithm::ends_with(sName, aExt))
                sResult.push_back(sName);
        }

        return sResult;
    }
}
