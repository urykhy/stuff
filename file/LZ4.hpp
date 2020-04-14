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

        bool filter(const char*& aBeginIn, const char* aEndIn,
                    char*& aBeginOut, char* aEndOut, bool aFlush)
        {
            size_t sSrcSize = aEndIn  - aBeginIn;
            size_t sDstSize = aEndOut - aBeginOut;
            const bool sEmptySrc = sSrcSize == 0;

            size_t rc = LZ4F_decompress(m_State, aBeginOut, &sDstSize, aBeginIn, &sSrcSize, nullptr);
            if (LZ4F_isError(rc))
                throw std::runtime_error("lz4: fail to decompress");

            aBeginIn  += sSrcSize;
            aBeginOut += sDstSize;

            if (rc > 0 and sEmptySrc)
                return false;               // no input and decompressor need input: end of stream
            return rc == 0 ? false : true;  // if rc == 0 - all decoded
        }
    };
    using Lz4DecompressorFilter = boost::iostreams::symmetric_filter<Lz4DecompressorImpl>;

    class Lz4CompressorImpl
    {
        LZ4F_compressionContext_t m_State;
        enum stage
        {
            NOT_STARTED,
            STARTED,
            DONE
        };
        const size_t MAX_INPUT_SIZE;
        stage m_Stage{NOT_STARTED};
        std::vector<uint8_t> m_Buffer;
        size_t m_Index = 0;

        void*  Xstart() { return &m_Buffer[m_Index]; }
        size_t Xsize() { return m_Buffer.size() - m_Index; }

        void create()
        {
            auto rc = LZ4F_createCompressionContext(&m_State, LZ4F_VERSION);
            if (LZ4F_isError(rc))
                throw std::runtime_error("lz4: fail to createCompressionContext");

            auto sChunk = LZ4F_compressBound(MAX_INPUT_SIZE, nullptr) + LZ4F_HEADER_SIZE_MAX + 4 /* endmark */;
            m_Buffer.resize(sChunk);
        }

        bool push(char*& aBeginOut, char* aEndOut)
        {
            size_t sDstSize = aEndOut - aBeginOut;
            size_t sSize = std::min(m_Index, sDstSize);
            memcpy(aBeginOut, m_Buffer.data(), sSize);
            aBeginOut += sSize;
            size_t sTail = m_Index - sSize;
            memmove(&m_Buffer[0], &m_Buffer[sSize], sTail);
            m_Index = sTail;
            return true;
        }

    public:
        using char_type = char;
        Lz4CompressorImpl(size_t aMaxInput) : MAX_INPUT_SIZE(aMaxInput) { create(); }
        ~Lz4CompressorImpl() { LZ4F_freeCompressionContext(m_State); }
        void close() { ;; }

        bool filter(const char*& aBeginIn, const char* aEndIn,
                    char*& aBeginOut, char* aEndOut, bool aFlush)
        {
            if (m_Index > 0)
                return push(aBeginOut, aEndOut);

            if (m_Stage == DONE)
                return false;  // compression done

            if (m_Stage == NOT_STARTED)
            {
                size_t rc = LZ4F_compressBegin(m_State, Xstart(), Xsize(), nullptr);
                if (LZ4F_isError(rc))
                    throw std::runtime_error("lz4: fail to compressBegin");
                m_Stage = STARTED;
                m_Index += rc;
            }

            size_t sSrcSize = aEndIn  - aBeginIn;
            sSrcSize = std::min(MAX_INPUT_SIZE, sSrcSize);
            const bool sEmptySrc = sSrcSize == 0;

            size_t rc = 0;
            if (sEmptySrc)
            {
                rc = LZ4F_compressEnd(m_State, Xstart(), Xsize(), nullptr);
                if (LZ4F_isError(rc))
                    throw std::runtime_error("lz4: fail to compressEnd");
                m_Index += rc;
                m_Stage = DONE;
            } else {
                rc = LZ4F_compressUpdate(m_State, Xstart(), Xsize(), aBeginIn, sSrcSize, nullptr);
                if (LZ4F_isError(rc))
                    throw std::runtime_error("lz4: fail to compressUpdate");
                aBeginIn += sSrcSize;
                m_Index  += rc;
            }
            return push(aBeginOut, aEndOut);
        }
    };
    using Lz4CompressorFilter = boost::iostreams::symmetric_filter<Lz4CompressorImpl>;

}
