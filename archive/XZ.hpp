#pragma once

#include <lzma.h>

#include "Interface.hpp"

namespace Archive {

    class ReadXZ : public IFilter
    {
        lzma_stream m_Stream = LZMA_STREAM_INIT;

    public:
        ReadXZ()
        {
            auto sRC = lzma_stream_decoder(&m_Stream, UINT64_MAX, LZMA_CONCATENATED);
            if (sRC != LZMA_OK)
                throw std::runtime_error("ReadXZ: fail to init decoder");
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            m_Stream.next_in   = (const uint8_t*)aSrc;
            m_Stream.avail_in  = aSrcLen;
            m_Stream.next_out  = (uint8_t*)aDst;
            m_Stream.avail_out = aDstLen;
            auto sRC           = lzma_code(&m_Stream, LZMA_RUN);
            if (sRC != LZMA_OK and sRC != LZMA_STREAM_END)
                throw std::runtime_error("ReadXZ: fail to decode");
            return {aSrcLen - m_Stream.avail_in, aDstLen - m_Stream.avail_out};
        }
        virtual ~ReadXZ() { lzma_end(&m_Stream); }
    };

    class WriteXZ : public IFilter
    {
        const uint32_t m_Level;
        bool           m_Reset  = false;
        lzma_stream    m_Stream = LZMA_STREAM_INIT;

        void init()
        {
            auto sRC = lzma_easy_encoder(&m_Stream, m_Level, LZMA_CHECK_CRC64);
            if (sRC != LZMA_OK)
                throw std::runtime_error("WriteXZ: fail to init encoder");
        }

        void cleanup()
        {
            lzma_end(&m_Stream);
        }

    public:
        WriteXZ(int aLevel = 3)
        : m_Level(aLevel)
        {
            init();
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            if (m_Reset) {
                cleanup();
                init();
                m_Reset = false;
            }
            m_Stream.next_in   = (const uint8_t*)aSrc;
            m_Stream.avail_in  = aSrcLen;
            m_Stream.next_out  = (uint8_t*)aDst;
            m_Stream.avail_out = aDstLen;
            auto sRC           = lzma_code(&m_Stream, LZMA_RUN);
            if (sRC != LZMA_OK)
                throw std::runtime_error("WriteXZ: fail to encode");
            return {aSrcLen - m_Stream.avail_in, aDstLen - m_Stream.avail_out};
        }
        Finish finish(char* aDst, size_t aDstLen) override
        {
            m_Stream.next_in   = nullptr;
            m_Stream.avail_in  = 0;
            m_Stream.next_out  = (uint8_t*)aDst;
            m_Stream.avail_out = aDstLen;
            auto sRC           = lzma_code(&m_Stream, LZMA_FINISH);
            if (sRC == LZMA_OK)
                return {aDstLen - m_Stream.avail_out, false};
            if (sRC == LZMA_STREAM_END) {
                m_Reset = true;
                return {aDstLen - m_Stream.avail_out, true};
            }
            throw std::runtime_error("WriteXZ: fail to finish");
        }
        virtual ~WriteXZ() { cleanup(); }
    };
} // namespace Archive