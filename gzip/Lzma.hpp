#pragma once

#include <boost/iostreams/filtering_stream.hpp>
#include <lzma.h>

namespace Gzip
{
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
}
