#pragma once

#include <lz4frame.h>

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
        bool                      m_Started{false};

        void check(const char* aMsg, size_t aCode)
        {
            if (LZ4F_isError(aCode))
                throw std::runtime_error("WriteLZ4: fail to " + std::string(aMsg) + ": " + std::string(LZ4F_getErrorName(aCode)));
        }

    public:
        WriteLZ4()
        {
            check("LZ4F_createCompressionContext", LZ4F_createCompressionContext(&m_State, LZ4F_VERSION));
        }

        size_t estimate(size_t aSize) override
        {
            if (!m_Started)
                return LZ4F_HEADER_SIZE_MAX;
            return LZ4F_compressBound(aSize, nullptr);
        }

        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            if (!m_Started) {
                if (aDstLen < LZ4F_HEADER_SIZE_MAX)
                    throw std::runtime_error("WriteLZ4: small output buffer");

                size_t sUsed = LZ4F_compressBegin(m_State, aDst, aDstLen, nullptr);
                check("LZ4F_compressBegin", sUsed);
                m_Started = true;
                return {0, sUsed};
            } else {
                aSrcLen = limitSrcLen(aSrcLen, aDstLen);
            }

            size_t sUsed = LZ4F_compressUpdate(m_State, aDst, aDstLen, aSrc, aSrcLen, nullptr);
            check("LZ4F_compressUpdate", sUsed);

            return Pair{aSrcLen, sUsed};
        }

        Finish finish(char* aDst, size_t aDstLen) override
        {
            if (aDstLen < estimate(0))
                throw std::runtime_error("WriteLZ4: small output buffer");

            size_t sUsed = LZ4F_compressEnd(m_State, aDst, aDstLen, nullptr);
            check("LZ4F_compressEnd", sUsed);
            m_Started = false;
            return Finish{sUsed, true};
        }

        virtual ~WriteLZ4()
        {
            LZ4F_freeCompressionContext(m_State);
        }

        size_t limitSrcLen(size_t aSrcLen, size_t aDstLen)
        {
            if (aDstLen >= estimate(aSrcLen))
                return aSrcLen;

            // ensure minimum size
            size_t sDstMinimal = estimate(64 * 1024);
            if (aDstLen < sDstMinimal)
                throw std::runtime_error("WriteLZ4: small output buffer");

            // reduce input size based on 64K block size
            size_t sDstBlockCount = aDstLen / sDstMinimal;
            aSrcLen = std::min(sDstBlockCount * 64 * 1024, aSrcLen);

            // ensure reduced size is ok
            if (aDstLen < estimate(aSrcLen))
                throw std::runtime_error("WriteLZ4: small output buffer");
            return aSrcLen;
        }
    };

} // namespace Archive