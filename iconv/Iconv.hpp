#pragma once

#include <string>
#include <iconv.h>

namespace Iconv
{
    class Convert
    {
        iconv_t m_Conv = nullptr;

        void reset()
        {
            iconv(m_Conv, nullptr, nullptr, nullptr, nullptr);
        }
    public:
        using Error = std::runtime_error;

        Convert(const std::string& aFrom, const std::string& aTo)
        {
            m_Conv = iconv_open(aTo.c_str(), aFrom.c_str());
            if ( m_Conv == (iconv_t)-1)
                throw Error("Iconv: init");
        }

        ~Convert()
        {
            iconv_close(m_Conv);
        }

        std::string operator()(const std::string& aText)
        {
            std::string sResult;
            const size_t sReserveSize = aText.size() * 4; // FIXME
            sResult.resize(sReserveSize);

            char*  inbuf = const_cast<char*>(&aText[0]);
            size_t inbytesleft = aText.size();
            char*  outbuf = &sResult[0];
            size_t outbytesleft = sResult.size();

            reset();
            while (inbytesleft > 0)
            {
                size_t rc = iconv(m_Conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
                if (-1 == rc)
                    throw Error("Iconv: convert");
            }
            sResult.resize(sReserveSize - outbytesleft);

            return sResult;
        }
    };
}
