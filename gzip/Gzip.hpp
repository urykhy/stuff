#pragma once

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/utility/string_ref.hpp>
#include <string_view>

#include "LZ4.hpp"
#include <file/File.hpp>

namespace Gzip
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

    // aHandler must return number of bytes processed
    template<class T>
    inline void by_chunk(const std::string& aName, T aHandler, size_t aBufferSize = 1024 * 1024)
    {
        namespace io = boost::iostreams;
        io::filtering_istream sStream;
        add_decompressor(sStream, get_format(File::get_extension(aName)), aBufferSize);

        std::ifstream sFile(aName);
        sFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        sStream.push(sFile);

        uint64_t sOffset = 0;
        std::string sBuffer(aBufferSize, '\0');
        while (sStream.good())
        {
            sStream.read(sBuffer.data() + sOffset, aBufferSize - sOffset);
            auto sLen = sStream.gcount();
            auto sUsed = aHandler(std::string_view(sBuffer.data(), sLen + sOffset));
            sOffset = sLen + sOffset - sUsed;
            memmove(sBuffer.data(), sBuffer.data() + sUsed, sOffset);
        }
    }

    class Decompress
    {
        std::string                         m_Result;
        boost::iostreams::filtering_ostream m_Filter;
        bool                                m_Active = true;

    public:
        Decompress() : m_Active(false) {}

        Decompress(FORMAT aFormat, size_t aBufferSize = 1024 * 1024)
        {
            add_decompressor(m_Filter, aFormat, aBufferSize);
            m_Filter.push(boost::iostreams::back_inserter(m_Result));
        }

        boost::string_ref operator()(const boost::string_ref compressed)
        {
            if (!m_Active)
                return compressed;
            m_Result.clear();
            if (compressed.empty())
                m_Filter.reset();
            else
                boost::iostreams::write(m_Filter, &compressed[0], compressed.size());
            return m_Result;
        }
    };

    inline std::string decode_buffer(FORMAT aFormat, boost::string_ref aInput)
    {
        std::string sResult;
        Decompress sUnpacker(aFormat);

        auto sChunk = sUnpacker(aInput);
        sResult.append(sChunk.data(), sChunk.size());
        sChunk = sUnpacker(""); /* EOF marker */
        sResult.append(sChunk.data(), sChunk.size());

        return sResult;
    }
}
