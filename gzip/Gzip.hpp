#pragma once

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/utility/string_ref.hpp>

#include <Lzma.hpp>
#include <LZ4.hpp>
#include <Zstd.hpp>

namespace Gzip
{
    enum {BUFFER_SIZE = 1024 * 64};

    class Decompress
    {
        std::string                         m_Result;
        boost::iostreams::filtering_ostream m_Filter;
        bool                                m_Active = true;

    public:
        Decompress() : m_Active(false) {}

        template<class T>
        Decompress(T t)
        {
            m_Filter.push(t, BUFFER_SIZE);
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
    using DecompressPtr = std::shared_ptr<Decompress>;

    enum FORMAT
    {
        PLAIN
      , GZIP
      , BZ2
      , XZ
      , LZ4
      , ZSTD
    };

    inline DecompressPtr make_unpacker(FORMAT aFormat)
    {
        switch (aFormat)
        {
            case FORMAT::PLAIN: return std::make_unique<Decompress>();
            case FORMAT::GZIP:  return std::make_unique<Decompress>(boost::iostreams::gzip_decompressor(boost::iostreams::gzip::default_window_bits, BUFFER_SIZE));
            case FORMAT::BZ2:   return std::make_unique<Decompress>(boost::iostreams::bzip2_decompressor(false, BUFFER_SIZE));
            case FORMAT::XZ:    return std::make_unique<Decompress>(LzmaDecompressorFilter(BUFFER_SIZE));
            case FORMAT::LZ4:   return std::make_unique<Decompress>(Lz4DecompressorFilter(BUFFER_SIZE));
            case FORMAT::ZSTD:  return std::make_unique<Decompress>(ZstdDecompressorFilter(BUFFER_SIZE));
        }
        throw std::runtime_error("unexpected format");
    }

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

    inline std::string decode_buffer(FORMAT aFormat, boost::string_ref aInput)
    {
        std::string sResult;
        auto sUnpacker = make_unpacker(aFormat);

        auto sChunk = (*sUnpacker)(aInput);
        sResult.append(sChunk.data(), sChunk.size());
        sChunk = (*sUnpacker)(""); /* EOF marker */
        sResult.append(sChunk.data(), sChunk.size());

        return sResult;
    }
}
