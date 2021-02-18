#pragma once

#include <regex>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <file/File.hpp>
#include <file/Tmp.hpp>
#include <parser/Autoindex.hpp>
#include <threads/SafeQueue.hpp>

#include "Curl.hpp"
#include "exception/Holder.hpp"

namespace Curl {
    inline File::Tmp download(Curl::Client::Request&& aRequest)
    {
        File::Tmp sTmp(File::getFilename(aRequest.url)); // FIXME: cut query
        Client    sClient;

        aRequest.callback = [&sTmp](void* aPtr, size_t aSize) -> size_t {
            sTmp.write(aPtr, aSize);
            return aSize;
        };

        int sStatus = sClient(aRequest).status;
        if (sStatus != 200)
            throw Client::Error("fail to download: http .status: " + std::to_string(sStatus));
        sTmp.flush();

        return sTmp;
    }

    // download without order. handler called from multiple threads
    void download(const Parser::StringList& aUrls, const Curl::Client::Request& aDefault, unsigned aCount, std::function<void(const std::string&, File::Tmp&)> aHandler)
    {
        Exception::Holder sError;

        Threads::SafeQueueThread<std::string> sQueue([&sError, &aDefault, &aHandler](const std::string& aUrl) {
            try {
                if (sError)
                    return;
                auto sRequest = aDefault; // inherit default params
                sRequest.url  = aUrl;
                auto sTmp     = download(std::move(sRequest));
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
            sleep(0.01);
            sError.raise();
        }
    }

    // pair.first is false if not modified
    inline std::pair<bool, Parser::StringList> index(const Curl::Client::Request& aRequest)
    {
        using L = Parser::StringList;
        Client sClient;
        auto   sResponse = sClient(aRequest);

        if (sResponse.status == 304)
            return std::make_pair(false, L{});
        if (sResponse.status != 200)
            throw Client::Error("fail to index: http status: " + std::to_string(sResponse.status));
        if (sResponse.body.empty())
            return std::make_pair(true, L{});

        return std::make_pair(true, Parser::Autoindex(sResponse.body));
    }

} // namespace Curl
