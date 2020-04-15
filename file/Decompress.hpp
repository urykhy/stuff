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
    namespace io = boost::iostreams;

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
    inline void add_decompressor(T& aStream, FORMAT aFormat)
    {
        switch (aFormat)
        {
            case FORMAT::PLAIN: break;
            case FORMAT::GZIP:  aStream.push(io::gzip_decompressor(io::gzip::default_window_bits));
                                break;
            case FORMAT::BZ2:   aStream.push(io::bzip2_decompressor(false));
                                break;
            case FORMAT::XZ:    aStream.push(io::lzma_decompressor());
                                break;
            case FORMAT::LZ4:   aStream.push(Lz4DecompressorFilter(65536), 65536);
                                break;
            case FORMAT::ZSTD:  aStream.push(io::zstd_decompressor());
                                break;
        }
    }

    template<class T>
    inline void add_compressor(T& aStream, FORMAT aFormat)
    {
        switch (aFormat)
        {
            case FORMAT::PLAIN: break;
            case FORMAT::GZIP:  aStream.push(io::gzip_compressor(io::gzip::default_window_bits));
                                break;
            case FORMAT::BZ2:   aStream.push(io::bzip2_compressor(false));
                                break;
            case FORMAT::XZ:    aStream.push(io::lzma_compressor());
                                break;
            case FORMAT::LZ4:   aStream.push(Lz4CompressorFilter(65536, 65536), 65536);
                                break;
            case FORMAT::ZSTD:  aStream.push(io::zstd_compressor());
                                break;
        }
    }

    template<class T>
    inline void read_file(const std::string& aName, T aHandler)
    {
        std::ifstream sFile(aName);
        sFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        io::filtering_istream sStream;
        add_decompressor(sStream, get_format(get_extension(aName)));
        sStream.push(sFile);
        sStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        aHandler(sStream);
    }

    template<class T>
    inline void write_file(const std::string& aName, T aHandler)
    {
        std::ofstream sFile;
        sFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        sFile.open(aName, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

        io::filtering_ostream sStream;
        add_compressor(sStream, get_format(get_extension(aName)));
        sStream.push(sFile);
        sStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        aHandler(sStream);
        sStream.flush();
        sStream.reset();
    }
} // namespace File