#pragma once

#include <boost/iostreams/filtering_stream.hpp>
#include <lz4frame.h>

namespace File
{
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
}
