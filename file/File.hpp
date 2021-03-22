#pragma once

#include <map>
#include <string>

#ifndef FILE_NO_ARCHIVE
#include <archive/Bzip2.hpp>
#include <archive/Gzip.hpp>
#include <archive/LZ4.hpp>
#include <archive/XZ.hpp>
#include <archive/Zstd.hpp>
#endif

#include "Reader.hpp"
#include "Util.hpp"
#include "Writer.hpp"

namespace File {
    enum FILTER
    {
        PLAIN,
        GZIP,
        BZ2,
        XZ,
        LZ4,
        ZSTD
    };

    inline FILTER getFilterType(const std::string& aName)
    {
        const static std::map<std::string, FILTER> sDict{
            {"gz", GZIP},
            {"bz2", BZ2},
            {"xz", XZ},
            {"lz4", LZ4},
            {"zst", ZSTD}};

        FILTER sFormat = PLAIN;
        auto   sIt     = sDict.find(getExtension(aName));
        if (sIt != sDict.end())
            sFormat = sIt->second;
#ifdef FILE_NO_ARCHIVE
        if (sFormat != PLAIN)
            throw std::runtime_error("File library without archive support");
#endif
        return sFormat;
    }

    inline Archive::FilterPtr makeReadFilter(FILTER aFormat)
    {
        switch (aFormat) {
#ifndef FILE_NO_ARCHIVE
        case GZIP: return std::make_unique<Archive::ReadGzip>(); break;
        case BZ2: return std::make_unique<Archive::ReadBzip2>(); break;
        case XZ: return std::make_unique<Archive::ReadXZ>(); break;
        case LZ4: return std::make_unique<Archive::ReadLZ4>(); break;
        case ZSTD: return std::make_unique<Archive::ReadZstd>(); break;
#endif
        default: return nullptr;
        }
    }

    inline Archive::FilterPtr makeWriteFilter(FILTER aFormat)
    {
        switch (aFormat) {
#ifndef FILE_NO_ARCHIVE
        case GZIP: return std::make_unique<Archive::WriteGzip>(); break;
        case BZ2: return std::make_unique<Archive::WriteBzip2>(); break;
        case XZ: return std::make_unique<Archive::WriteXZ>(); break;
        case LZ4: return std::make_unique<Archive::WriteLZ4>(); break;
        case ZSTD: return std::make_unique<Archive ::WriteZstd>(); break;
#endif
        default: return nullptr;
        }
    }

    template <class T>
    inline void read(const std::string& aName, T&& aHandler)
    {
        FileReader sFile(aName);
        auto       sType = getFilterType(aName);

        if (sType == PLAIN) {
            BufReader sReader(&sFile);
            aHandler(&sReader);
        } else {
            auto         sFilter = makeReadFilter(sType);
            FilterReader sReader(&sFile, sFilter.get());
            aHandler(&sReader);
        }
    }

    template <class T>
    inline void write(const std::string& aName, T&& aHandler, int aFlags = 0)
    {
        FileWriter sFile(aName, aFlags);
        auto       sType = getFilterType(aName);

        if (sType == PLAIN) {
            BufWriter sWriter(&sFile);
            aHandler(&sWriter);
        } else {
            auto         sFilter = makeWriteFilter(sType);
            FilterWriter sWriter(&sFile, sFilter.get());
            aHandler(&sWriter);
        }
    }

    // read helpers

    template <class T>
    inline void by_chunk(const std::string& aName, T&& aHandler, size_t aBufferSize = DEFAULT_BUFFER_SIZE)
    {
        read(aName, [aHandler, aBufferSize](IReader* aReader) mutable {
            std::string sTmp(aBufferSize, ' ');
            while (not aReader->eof()) {
                size_t sLen = aReader->read(sTmp.data(), sTmp.size());
                aHandler(std::string_view(sTmp.data(), sLen));
            }
        });
    }

    inline std::string to_string(const std::string& aName)
    {
        std::string sResult;
        by_chunk(aName, [&](const std::string_view aStr) mutable {
            sResult.append(aStr.begin(), aStr.end());
        });
        return sResult;
    }

    template <class T>
    void by_string(const std::string& aName, T aHandler)
    {
        std::string sBuf;
        by_chunk(aName, [&](const std::string_view aStr) {
            sBuf.append(aStr.begin(), aStr.end());
            size_t sPos = 0;
            while (true) {
                size_t sNL = sBuf.find('\n', sPos);
                if (sNL != std::string::npos) {
                    aHandler(std::string_view(&sBuf[sPos], sNL - sPos));
                    sPos = sNL + 1;
                } else {
                    break;
                }
            }
            sBuf.erase(0, sPos);
        });
        if (!sBuf.empty())
            aHandler(std::string_view(sBuf));
    }

} // namespace File