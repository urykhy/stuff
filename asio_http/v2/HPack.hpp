#pragma once

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "Types.hpp"

#include <container/Stream.hpp>
#include <nghttp2/nghttp2.h>
#include <parser/Atoi.hpp>
#include <parser/Url.hpp>
#include <string/String.hpp>

namespace asio_http::v2 {

    class Inflate
    {
        using InflaterT = std::unique_ptr<nghttp2_hd_inflater, void (*)(nghttp2_hd_inflater*)>;
        InflaterT m_Ptr;

        template <class T>
        void collect(nghttp2_nv sHeader, T& aObject)
        {
            std::string sName((const char*)sHeader.name, sHeader.namelen);
            std::string sValue((const char*)sHeader.value, sHeader.valuelen);
            aObject->set_header(sName, sValue);
        }

    public:
        Inflate()
        : m_Ptr([]() {
            nghttp2_hd_inflater* sTmp = nullptr;
            if (nghttp2_hd_inflate_new(&sTmp))
                throw std::runtime_error("fail to initialize nghttp/inflater");
            return InflaterT(sTmp, nghttp2_hd_inflate_del);
        }())
        {
        }

        template <class T>
        void operator()(std::string_view aStr, T& aObject)
        {
            while (!aStr.empty()) {
                nghttp2_nv sHeader;
                int        sFlags = 0;

                int sUsed = nghttp2_hd_inflate_hd2(m_Ptr.get(),
                                                   &sHeader,
                                                   &sFlags,
                                                   (const uint8_t*)aStr.data(),
                                                   aStr.size(),
                                                   sHeader.flags & Flags::END_HEADERS);

                if (sUsed < 0)
                    throw std::invalid_argument("fail to inflate headers");
                aStr.remove_prefix(sUsed);
                if (sFlags & NGHTTP2_HD_INFLATE_EMIT)
                    collect(sHeader, aObject);
                if (sFlags & NGHTTP2_HD_INFLATE_FINAL)
                    nghttp2_hd_inflate_end_headers(m_Ptr.get());
            }
        }

        template <class T>
        void operator()(const Header& aHeader, std::string_view aData, T& aObject)
        {
            Container::imemstream sData(aData);

            uint8_t sPadLength = 0;
            if (aHeader.flags & Flags::PADDED)
                sData.read(sPadLength);
            if (aHeader.flags & Flags::PRIORITY) {
                sData.skip(4 + 1); // stream id + prio
            }
            std::string_view sRest = sData.rest();
            sRest.remove_suffix(sPadLength);
            operator()(sRest, aObject);
        }
    };

    class Deflate
    {
        using DeflaterT = std::unique_ptr<nghttp2_hd_deflater, void (*)(nghttp2_hd_deflater*)>;
        DeflaterT m_Deflate;

        std::string deflate(const std::vector<nghttp2_nv>& aPairs)
        {
            std::string sTmp;
            size_t      sLength = nghttp2_hd_deflate_bound(m_Deflate.get(), aPairs.data(), aPairs.size());
            sTmp.resize(sLength);
            ssize_t sUsed = nghttp2_hd_deflate_hd(m_Deflate.get(),
                                                  (uint8_t*)sTmp.data(),
                                                  sLength,
                                                  aPairs.data(),
                                                  aPairs.size());
            if (sUsed < 0)
                throw std::runtime_error("fail to deflate headers");
            sTmp.resize(sUsed);
            return sTmp;
        }

        struct Pairs
        {
            std::list<std::string>  m_Shadow;
            std::vector<nghttp2_nv> m_Pairs;

            void assign_ref(boost::beast::string_view sName, boost::beast::string_view sValue)
            {
                nghttp2_nv sNV;
                sNV.name     = (uint8_t*)sName.data();
                sNV.namelen  = sName.size();
                sNV.value    = (uint8_t*)sValue.data();
                sNV.valuelen = sValue.size();
                sNV.flags    = NGHTTP2_NV_FLAG_NONE;
                m_Pairs.push_back(sNV);
            }

            void assign_lower(boost::beast::string_view sName, boost::beast::string_view sValue)
            {
                m_Shadow.push_back(sName.to_string());
                String::tolower(m_Shadow.back());
                assign_ref(m_Shadow.back(), sValue);
            }
        };

    public:
        Deflate()
        : m_Deflate([]() {
            nghttp2_hd_deflater* sTmp = nullptr;
            if (nghttp2_hd_deflate_new(&sTmp, DEFAULT_HEADER_TABLE_SIZE))
                throw std::runtime_error("fail to initialize nghttp/deflater");
            return DeflaterT(sTmp, nghttp2_hd_deflate_del);
        }())
        {
        }

        std::string operator()(const ClientRequest& aRequest)
        {
            Pairs sPairs;

            sPairs.assign_ref(":method", http::to_string(aRequest.method));
            const auto sParsed = Parser::url(aRequest.url);
            sPairs.assign_ref(":path", sParsed.path);
            sPairs.assign_ref(":scheme", "http");
            sPairs.assign_ref(":authority", sParsed.host); // FIXME: add port

            for (auto& x : aRequest.headers)
                sPairs.assign_lower(x.first, x.second);

            return deflate(sPairs.m_Pairs);
        }

        std::string operator()(const Response& aResponse)
        {
            Pairs sPairs;

            const std::string sStatus = std::to_string(aResponse.result_int());
            sPairs.assign_ref(":status", sStatus);

            for (auto& x : aResponse)
                sPairs.assign_lower(x.name_string(), x.value());

            return deflate(sPairs.m_Pairs);
        }
    };
} // namespace asio_http::v2