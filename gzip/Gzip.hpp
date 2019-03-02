#pragma once

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <lzma.h>
#include <lz4frame.h>
#include <zstd.h>
#include <boost/utility/string_ref.hpp>

#include <File.hpp>

namespace Gzip
{
    enum {BUFFER_SIZE = 1024 * 64};

    class LzmaDecompressorImpl
    {
        lzma_stream m_Stream = LZMA_STREAM_INIT;

        void create()
        {
            lzma_ret ret = lzma_stream_decoder(&m_Stream, UINT64_MAX, 0);
            if (ret != LZMA_OK)
                throw std::runtime_error("fail to init lzma decoder");
        }
    public:

        using char_type = char;
        LzmaDecompressorImpl()  { create(); }
        void close()            { create(); }   // It is OK to reuse lzma_stream with different initialization function without calling lzma_end() first.
        ~LzmaDecompressorImpl() { lzma_end(&m_Stream); }

        bool filter( const char*& aBeginIn, const char* aEndIn,
                     char*& aBeginOut, char* aEndOut, bool aFlush )
        {
            lzma_action sAction = LZMA_RUN;
            m_Stream.next_in = (const uint8_t*)aBeginIn;
            m_Stream.avail_in = aEndIn - aBeginIn;
            m_Stream.next_out = (uint8_t*)aBeginOut;
            m_Stream.avail_out = aEndOut - aBeginOut;

            lzma_ret ret = lzma_code(&m_Stream, sAction);

            aBeginIn = (const char*)m_Stream.next_in;
            aBeginOut = (char*)m_Stream.next_out;

            if (ret == LZMA_OK)
                return true;
            if (ret == LZMA_STREAM_END)
                return false;
            throw std::runtime_error("fail to lzma_code");
        }
    };
    using LzmaDecompressorFilter = boost::iostreams::symmetric_filter<LzmaDecompressorImpl>;

    // read files after lz4 tool
    // LZ4IO_decompressLZ4F
    class Lz4DecompressorImpl
    {
        LZ4F_decompressionContext_t m_State;
        void create()
        {
            auto rc = LZ4F_createDecompressionContext(&m_State, LZ4F_VERSION);
            if (LZ4F_isError(rc))
                throw std::runtime_error("lz4: fail to createDecompressionContext");
        }

    public:

        using char_type = char;

        Lz4DecompressorImpl()  { create(); }
        ~Lz4DecompressorImpl() { LZ4F_freeDecompressionContext(m_State); }
        void close() { ;; }

        bool filter( const char*& aBeginIn, const char* aEndIn,
                     char*& aBeginOut, char* aEndOut, bool aFlush )
        {
            size_t sSrcSize = aEndIn - aBeginIn;
            size_t sDstSize = aEndOut - aBeginOut;

            auto rc = LZ4F_decompress(m_State, aBeginOut, &sDstSize, aBeginIn, &sSrcSize, nullptr);

            if (LZ4F_isError(rc))
                throw std::runtime_error("lz4: fail to decompress");

            if (0 == sSrcSize and 0 == sDstSize and aFlush)
                return false;

            aBeginIn += sSrcSize;
            aBeginOut += sDstSize;

            if (rc > 0)
                return true;    // expect more data
            return false;       // frame decoded
        }
    };
    using Lz4DecompressorFilter = boost::iostreams::symmetric_filter<Lz4DecompressorImpl>;

    class ZstdDecompressorImpl
    {
        ZSTD_DStream* m_State = nullptr;
        bool m_Done = false;
        void create()
        {
            m_State = ZSTD_createDStream();
            if (!m_State)
                throw std::runtime_error("zstd: fail to ZSTD_createDStream");
            auto rc = ZSTD_initDStream(m_State);
            if (ZSTD_isError(rc))
                throw std::runtime_error("zstd: fail to ZSTD_initDStream");
        }
    public:
        using char_type = char;

        ZstdDecompressorImpl() { create(); }
        ~ZstdDecompressorImpl() { ZSTD_freeDStream(m_State); }

        void close() { ;; }
        bool filter( const char*& aBeginIn, const char* aEndIn,
                     char*& aBeginOut, char* aEndOut, bool aFlush )
        {
            if (m_Done)
                return false;

            size_t inputSize = aEndIn - aBeginIn;
            size_t outputSize = aEndOut - aBeginOut;
            ZSTD_inBuffer in{aBeginIn, inputSize, 0};
            ZSTD_outBuffer out{aBeginOut, outputSize, 0};

            const auto rc =  ZSTD_decompressStream(m_State, &out, &in);

            if (ZSTD_isError(rc))
                throw std::runtime_error("zstd: fail to decompress");
            if (out.pos == 0 and aFlush)
                throw std::runtime_error("zstd: fail to make progress at flush");

            aBeginIn += in.pos;
            aBeginOut += out.pos;

            if (rc == 0)
            {
                m_Done = true;
                return false; // frame decoded
            }
            return true; // expect more data
        }
    };
    using ZstdDecompressorFilter = boost::iostreams::symmetric_filter<ZstdDecompressorImpl>;

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

    inline DecompressPtr make_unpacker(const std::string& aFilename)
    {
        auto sExtension = File::get_extension(aFilename);

        if (sExtension == "gz")
            return std::make_shared<Decompress>(boost::iostreams::gzip_decompressor(boost::iostreams::gzip::default_window_bits, BUFFER_SIZE));
        else if (sExtension == "bz2")
            return std::make_shared<Decompress>(boost::iostreams::bzip2_decompressor(false, BUFFER_SIZE));
        else if (sExtension == "xz")
            return std::make_shared<Decompress>(LzmaDecompressorFilter(BUFFER_SIZE));
        else if (sExtension == "lz4")
            return std::make_shared<Decompress>(Lz4DecompressorFilter(BUFFER_SIZE));
        else if (sExtension == "zst")
            return std::make_shared<Decompress>(ZstdDecompressorFilter(BUFFER_SIZE));
        else return std::make_shared<Decompress>();  // no decompression
    }
}
