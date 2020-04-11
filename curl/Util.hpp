#pragma once

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

} // namespace Curl
