#pragma once

#include <regex>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "Curl.hpp"
#include <file/File.hpp>
#include <file/Tmp.hpp>

namespace Curl
{
    inline File::Tmp download(const std::string& aUrl, const Curl::Client::Params& aParams)
    {
        File::Tmp sTmp(File::get_filename(aUrl));   // FIXME: cut query
        Client sClient(aParams);

        int sCode = sClient.GET(aUrl, [&sTmp](void* aPtr, size_t aSize) -> size_t {
            sTmp.write(aPtr, aSize);
            return aSize;
        });
        if (sCode != 200)
            throw Client::Error("fail to download: http code: " + std::to_string(sCode));

        return sTmp;
    }

    using FileList = std::list<std::string>;

    inline FileList parse_autoindex(const std::string& aStr)
    {
        FileList sTmp;
        boost::algorithm::split(sTmp, aStr, boost::is_any_of("\n"), boost::token_compress_on);

        FileList sResult;
        const std::regex sRegexp("<a href=\"(.*)\">\\1</a>");
        for (auto& x : sTmp)
        {
            std::smatch sMatch;
            if (std::regex_search(x, sMatch, sRegexp))
                sResult.push_back(sMatch[1]);
        }
        return sResult;
    }

    inline FileList index(const std::string& aUrl, const Curl::Client::Params& aParams, time_t aIMS = 0)
    {
        Client sClient(aParams);
        auto [sCode, sStr] = sClient.GET(aUrl, aIMS);

        if (sCode == 304)
            return FileList{};
        if (sCode != 200)
            throw Client::Error("fail to index: http code: " + std::to_string(sCode));
        if (sStr.empty())
            return FileList{};

        return parse_autoindex(sStr);
    }

} // namespace Curl
