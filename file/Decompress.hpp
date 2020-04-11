#pragma once

#include <map>
#include <string>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filter/zstd.hpp>

#include "LZ4.hpp"
#include "Util.hpp"

namespace File
{
    enum FORMAT
    {
        PLAIN
      , GZIP
      , BZ2
      , XZ
      , LZ4
      , ZSTD
    };

    inline FORMAT get_format(const std::string& aExtension)
    {
        const static std::map<std::string, FORMAT> sDict {
            {"gz",  FORMAT::GZIP}
          , {"bz2", FORMAT::BZ2}
          , {"xz",  FORMAT::XZ}
          , {"lz4", FORMAT::LZ4}
          , {"zst", FORMAT::ZSTD}
        };

        FORMAT sFormat = FORMAT::PLAIN;
        auto sIt = sDict.find(aExtension);
        if (sIt != sDict.end())
            sFormat = sIt->second;
        return sFormat;
    }

    template<class T>
    inline void add_decompressor(T& aStream, FORMAT aFormat, size_t aBufferSize)
    {
        namespace io = boost::iostreams;
        switch (aFormat)
        {
            case FORMAT::PLAIN: break;
            case FORMAT::GZIP:  aStream.push(io::gzip_decompressor(io::gzip::default_window_bits, aBufferSize));
                                break;
            case FORMAT::BZ2:   aStream.push(io::bzip2_decompressor(false, aBufferSize), aBufferSize);
                                break;
            case FORMAT::XZ:    aStream.push(io::lzma_decompressor(aBufferSize), aBufferSize);
                                break;
            case FORMAT::LZ4:   aStream.push(Lz4DecompressorFilter(aBufferSize), aBufferSize);
                                break;
            case FORMAT::ZSTD:  aStream.push(io::zstd_decompressor(aBufferSize), aBufferSize);
                                break;
        }
    }

    template<class T>
    inline void with_file(const std::string& aName, T aHandler, size_t aBufferSize)
    {
        namespace io = boost::iostreams;
        io::filtering_istream sStream;
        add_decompressor(sStream, get_format(get_extension(aName)), aBufferSize);

        std::ifstream sFile(aName);
        sFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        sStream.push(sFile);

        aHandler(sStream);
    }
} // namespace File