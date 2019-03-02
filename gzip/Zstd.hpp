#pragma once

#include <boost/iostreams/filtering_stream.hpp>
#include <zstd.h>

namespace Gzip
{
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
}
