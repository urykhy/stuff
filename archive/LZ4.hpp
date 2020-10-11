#pragma once

#include <lz4.h>

#include "Interface.hpp"

namespace Archive {

    class ReadLZ4 : public IFilter
    {
        LZ4F_decompressionContext_t m_State;

        void check(const char* aMsg, size_t aCode)
        {
            if (LZ4F_isError(aCode))
                throw std::runtime_error("ReadLZ4: fail to " + std::string(aMsg) + ": " + std::string(LZ4F_getErrorName(aCode)));
        }

    public:
        ReadLZ4()
        {
            check("createDecompressionContext", LZ4F_createDecompressionContext(&m_State, LZ4F_VERSION));
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            check("LZ4F_decompress", LZ4F_decompress(m_State, aDst, &aDstLen, aSrc, &aSrcLen, nullptr));
            return {aSrcLen, aDstLen};
        }
        Finish finish(char* aDst, size_t aDstLen) override
        {
            size_t sDstAvail = aDstLen;
            size_t sSrcLen   = 0;
            check("LZ4F_decompress", LZ4F_decompress(m_State, aDst, &aDstLen, nullptr, &sSrcLen, nullptr));
            return {aDstLen, aDstLen < sDstAvail};
        }
        virtual ~ReadLZ4() { LZ4F_freeDecompressionContext(m_State); }
    };

    class WriteLZ4 : public IFilter
    {
        LZ4F_compressionContext_t m_State;

        enum
        {
            BUFFER_SIZE = 64 * 1024
        };

        std::string m_InputBuffer; // plain input data
        size_t      m_InputEnd = 0;
        std::string m_Buffer; // compressed data
        size_t      m_Pos = 0;
        size_t      m_End = 0;

        void check(const char* aMsg, size_t aCode)
        {
            if (LZ4F_isError(aCode))
                throw std::runtime_error("WriteLZ4: fail to " + std::string(aMsg) + ": " + std::string(LZ4F_getErrorName(aCode)));
        }

        void start_i()
        {
            size_t sRC = LZ4F_compressBegin(m_State, &m_Buffer[0], m_Buffer.size(), nullptr);
            check("LZ4F_compressBegin", sRC);
            m_End   = sRC;
            m_Stage = STARTED;
        }

        void finish_i()
        {
            size_t sRC = LZ4F_compressEnd(m_State, &m_Buffer[0], m_Buffer.size(), nullptr);
            check("LZ4F_compressEnd", sRC);
            m_End   = sRC;
            m_Stage = FINISHED;
        }

        enum State
        {
            IDLE,
            STARTED,
            FINISHED
        };
        unsigned m_Stage = IDLE;

        Pair flushBuffer(char* aDst, size_t aDstLen)
        {
            // flush previous data
            size_t sMin = std::min(m_End - m_Pos, aDstLen);
            memcpy(aDst, &m_Buffer[m_Pos], sMin);
            m_Pos += sMin;
            if (m_Pos == m_End) {
                m_Pos = 0;
                m_End = 0;
            }
            return {0, sMin};
        }

        size_t collectInput(const char* aSrc, size_t aSrcLen)
        {
            size_t sMin = std::min(BUFFER_SIZE - m_InputEnd, aSrcLen);
            memcpy(&m_InputBuffer[m_InputEnd], aSrc, sMin);
            m_InputEnd += sMin;
            return sMin;
        }

        void compressBuffer()
        {
            size_t sRC = LZ4F_compressUpdate(m_State, &m_Buffer[0], m_Buffer.size(), &m_InputBuffer[0], m_InputEnd, nullptr);
            check("LZ4F_compressUpdate", sRC);
            m_InputEnd = 0;
            m_End      = sRC;
        }

    public:
        WriteLZ4()
        : m_InputBuffer(BUFFER_SIZE, ' ')
        , m_Buffer(LZ4F_compressBound(BUFFER_SIZE, nullptr), ' ')
        {
            check("LZ4F_createCompressionContext", LZ4F_createCompressionContext(&m_State, LZ4F_VERSION));
        }

        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            if (m_End > 0)
                return flushBuffer(aDst, aDstLen);

            if (m_Stage == IDLE) {
                start_i();
                return flushBuffer(aDst, aDstLen);
            }

            Pair sResult;
            // collect full 64kb block
            if (m_InputEnd < BUFFER_SIZE)
                sResult.usedSrc = collectInput(aSrc, aSrcLen);

            if (m_InputEnd == BUFFER_SIZE)
                compressBuffer();

            return sResult;
        }

        Finish finish(char* aDst, size_t aDstLen) override
        {
            if (m_End > 0)
                return {flushBuffer(aDst, aDstLen).usedDst, false};

            if (m_InputEnd != 0) {
                compressBuffer();
                return {flushBuffer(aDst, aDstLen).usedDst, false};
            }

            if (m_Stage == STARTED) {
                finish_i();
                return {flushBuffer(aDst, aDstLen).usedDst, false};
            }

            return {0, true};
        }

        virtual ~WriteLZ4()
        {
            LZ4F_freeCompressionContext(m_State);
        }
    };

} // namespace Archive