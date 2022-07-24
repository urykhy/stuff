#pragma once

#include <zlib.h>

#include "Interface.hpp"

namespace Archive {
    class ReadGzip : public IFilter
    {
        z_stream m_State;

    public:
        ReadGzip()
        {
            memset(&m_State, 0, sizeof(m_State));
            auto sRC = inflateInit(&m_State);
            if (sRC != Z_OK)
                throw std::runtime_error("ReadGzip: fail to init inflate");
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            m_State.next_in   = (uint8_t*)aSrc;
            m_State.avail_in  = aSrcLen;
            m_State.next_out  = (uint8_t*)aDst;
            m_State.avail_out = aDstLen;
            auto sRC          = inflate(&m_State, Z_NO_FLUSH);
            if (sRC != Z_OK and sRC != Z_STREAM_END)
                throw std::runtime_error("ReadGzip: fail to inflate: " + std::to_string(sRC));
            if (sRC == Z_STREAM_END) {
                inflateReset(&m_State);
            }
            return {aSrcLen - m_State.avail_in, aDstLen - m_State.avail_out};
        }
        virtual ~ReadGzip() { inflateEnd(&m_State); }
    };

    class WriteGzip : public IFilter
    {
        const int m_Level;
        bool      m_Reset = false;
        z_stream  m_State;

        void init()
        {
            memset(&m_State, 0, sizeof(m_State));
            auto sRC = deflateInit(&m_State, m_Level);
            if (sRC != Z_OK)
                throw std::runtime_error("WriteGzip: fail to init deflate");
        }

        void cleanup()
        {
            deflateEnd(&m_State);
        }

    public:
        WriteGzip(int aLevel = 3)
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
            m_State.next_in   = (uint8_t*)aSrc;
            m_State.avail_in  = aSrcLen;
            m_State.next_out  = (uint8_t*)aDst;
            m_State.avail_out = aDstLen;
            auto sRC          = deflate(&m_State, Z_NO_FLUSH);
            if (sRC != Z_OK)
                throw std::runtime_error("WriteGzip: fail to deflate");
            return {aSrcLen - m_State.avail_in, aDstLen - m_State.avail_out};
        }
        Finish finish(char* aDst, size_t aDstLen) override
        {
            m_State.next_in   = nullptr;
            m_State.avail_in  = 0;
            m_State.next_out  = (uint8_t*)aDst;
            m_State.avail_out = aDstLen;
            auto sRC          = deflate(&m_State, Z_FINISH);
            if (sRC == Z_OK or sRC == Z_BUF_ERROR)
                return {aDstLen - m_State.avail_out, false};
            if (sRC == Z_STREAM_END) {
                m_Reset = true;
                return {aDstLen - m_State.avail_out, true};
            }
            throw std::runtime_error("WriteGzip: fail to deflate/finish");
        }
        virtual ~WriteGzip() { cleanup(); }
    };
} // namespace Archive