#pragma once

#include <regex>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "Curl.hpp"
#include <file/File.hpp>
#include <file/Tmp.hpp>
#include <parser/Autoindex.hpp>
#include <threads/SafeQueue.hpp>
#include "exception/Holder.hpp"

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

    // download without order. handler called from multiple threads
    void download(const Parse::StringList& aUrls, const Curl::Client::Params& aParams, unsigned aCount, std::function<void(const std::string&, File::Tmp&)> aHandler)
    {
        Exception::Holder sError;

        Threads::SafeQueueThread<std::string> sQueue([&sError, &aParams, &aHandler](const std::string& aUrl)
        {
            try {
                if (sError) return;
                auto sTmp = download(aUrl, aParams);
                if (sError) return;
                aHandler(aUrl, sTmp);
            } catch (...) {
                sError.collect();
            }
        });

        for (const std::string& sUrl : aUrls)
            sQueue.insert(sUrl);

        Threads::Group sGroup;
        sQueue.start(sGroup, aCount);

        while(!sQueue.idle())
        {
            sleep(0.01);
            sError.raise();
        }
    }

    // pair.first is false if not modified
    inline std::pair<bool, Parse::StringList> index(const std::string& aUrl, const Curl::Client::Params& aParams, time_t aIMS = 0)
    {
        using L = Parse::StringList;
        Client sClient(aParams);
        auto [sCode, sStr] = sClient.GET(aUrl, aIMS);

        if (sCode == 304)
            return std::make_pair(false, L{});
        if (sCode != 200)
            throw Client::Error("fail to index: http code: " + std::to_string(sCode));
        if (sStr.empty())
            return std::make_pair(true, L{});

        return std::make_pair(true, Parse::Autoindex(sStr));
    }

} // namespace Curl
