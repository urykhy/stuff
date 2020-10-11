#pragma once

#include <bzlib.h>
#include <string.h>

#include "Interface.hpp"

namespace Archive {
    class ReadBzip2 : public IFilter
    {
        bz_stream m_State;

    public:
        ReadBzip2()
        {
            memset(&m_State, 0, sizeof(m_State));
            auto sRC = BZ2_bzDecompressInit(&m_State, 0 /* verbose*/, 0 /* small */);
            if (sRC != BZ_OK)
                throw std::runtime_error("ReadBzip2: fail to bzDecompressInit");
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            m_State.next_in   = (char*)aSrc;
            m_State.avail_in  = aSrcLen;
            m_State.next_out  = aDst;
            m_State.avail_out = aDstLen;
            auto sRC          = BZ2_bzDecompress(&m_State);
            if (sRC != BZ_OK and sRC != BZ_STREAM_END)
                throw std::runtime_error("ReadBzip2: fail to bzDecompress: " + std::to_string(sRC));
            Pair sResult {aSrcLen - m_State.avail_in, aDstLen - m_State.avail_out};
            if (sRC == BZ_STREAM_END)
            {
                BZ2_bzDecompressEnd(&m_State);
                sRC = BZ2_bzDecompressInit(&m_State, 0 /* verbose*/, 0 /* small */);
                if (sRC != BZ_OK)
                    throw std::runtime_error("ReadBzip2: fail to bzDecompressInit");
            }
            return sResult;
        }
        virtual ~ReadBzip2() { BZ2_bzDecompressEnd(&m_State); }
    };

    class WriteBzip2 : public IFilter
    {
        bz_stream m_State;

    public:
        WriteBzip2(int aLevel = 3)
        {
            memset(&m_State, 0, sizeof(m_State));
            auto sRC = BZ2_bzCompressInit(&m_State, aLevel /* block size*/, 0 /* verbose */, 30 /* work factor */);
            if (sRC != BZ_OK)
                throw std::runtime_error("WriteBzip2: fail to bzCompressInit");
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            m_State.next_in   = (char*)aSrc;
            m_State.avail_in  = aSrcLen;
            m_State.next_out  = aDst;
            m_State.avail_out = aDstLen;
            auto sRC          = BZ2_bzCompress(&m_State, BZ_RUN);
            if (sRC != BZ_RUN_OK)
                throw std::runtime_error("WriteBzip2: fail to bzCompress");
            return {aSrcLen - m_State.avail_in, aDstLen - m_State.avail_out};
        }
        Finish finish(char* aDst, size_t aDstLen) override
        {
            m_State.next_in   = nullptr;
            m_State.avail_in  = 0;
            m_State.next_out  = aDst;
            m_State.avail_out = aDstLen;
            auto sRC          = BZ2_bzCompress(&m_State, BZ_FINISH);
            if (sRC != BZ_FINISH_OK and sRC != BZ_STREAM_END)
                throw std::runtime_error("WriteBzip2: fail to bzCompress/finish: " + std::to_string(sRC));
            return {aDstLen - m_State.avail_out, sRC == BZ_STREAM_END};
        }
        virtual ~WriteBzip2() { BZ2_bzCompressEnd(&m_State); }
    };
} // namespace Archive