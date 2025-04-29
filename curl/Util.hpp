#pragma once

#include <regex>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "Curl.hpp"
#include "exception/Holder.hpp"

#include <file/File.hpp>
#include <file/Tmp.hpp>
#include <parser/Autoindex.hpp>
#include <threads/SafeQueue.hpp>

namespace Curl {
    inline File::Tmp download(const std::string& aUrl, const Curl::Client::Default& aParams = {})
    {
        File::Tmp sTmp(File::getFilename(aUrl)); // FIXME: cut query
        Client    sClient;

        auto sCallback = [&sTmp](void* aPtr, size_t aSize) -> size_t {
            sTmp.write(aPtr, aSize);
            return aSize;
        };

        int sStatus = sClient(
                          aParams.wrap(
                              {.method   = Curl::Client::Method::GET,
                               .url      = aUrl,
                               .callback = sCallback}))
                          .status;
        if (sStatus != 200)
            throw Client::Error("fail to download: http .status: " + std::to_string(sStatus));
        sTmp.flush();

        return sTmp;
    }

    // download without order. handler called from multiple threads
    void download(const Parser::StringList& aUrls, unsigned aCount, std::function<void(const std::string&, File::Tmp&)> aHandler, const Curl::Client::Default& aParams = {})
    {
        Exception::Holder sError;

        Threads::SafeQueueThread<std::string> sQueue([&sError, &aParams, &aHandler](const std::string& aUrl) {
            try {
                if (sError)
                    return;
                auto sTmp = download(aUrl, aParams);
                if (sError)
                    return;
                aHandler(aUrl, sTmp);
            } catch (...) {
                sError.collect();
            }
        });

        for (const std::string& sUrl : aUrls)
            sQueue.insert(sUrl);

        Threads::Group sGroup;
        sQueue.start(sGroup, aCount);

        while (!sQueue.idle()) {
            Threads::sleep(0.01);
            sError.raise();
        }
    }

    // pair.first is false if not modified
    inline std::pair<bool, Parser::StringList> index(const std::string& aUrl, const time_t aIMS = 0, const Curl::Client::Default& aParams = {})
    {
        using L = Parser::StringList;
        Client sClient;

        auto sResponse = sClient(
            aParams.wrap(
                {.method = Curl::Client::Method::GET,
                 .url    = aUrl,
                 .ims    = aIMS}));

        if (sResponse.status == 304)
            return std::make_pair(false, L{});
        if (sResponse.status != 200)
            throw Client::Error("fail to index: http status: " + std::to_string(sResponse.status));
        if (sResponse.body.empty())
            return std::make_pair(true, L{});

        return std::make_pair(true, Parser::Autoindex(sResponse.body));
    }

} // namespace Curl
