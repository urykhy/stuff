#pragma once

#include <zstd.h>

#include <unsorted/Raii.hpp>

#include "Interface.hpp"

namespace Archive {

    class ReadZstd : public IFilter
    {
        ZSTD_DStream* m_State = nullptr;

        void check(const char* aMsg, size_t aCode)
        {
            if (ZSTD_isError(aCode))
                throw std::runtime_error("ReadZstd: fail to " + std::string(aMsg) + ": " + std::string(ZSTD_getErrorName(aCode)));
        }

    public:
        ReadZstd()
        {
            Util::Raii sGuard([this]() { ZSTD_freeDStream(m_State); });
            m_State = ZSTD_createDStream();
            if (!m_State)
                throw std::runtime_error("ReadZstd: fail to createDStream");
            check("initDStream", ZSTD_initDStream(m_State));
            sGuard.dismiss();
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            ZSTD_inBuffer  sSrc{aSrc, aSrcLen, 0};
            ZSTD_outBuffer sDst{aDst, aDstLen, 0};
            check("decompressStream", ZSTD_decompressStream(m_State, &sDst, &sSrc));
            return {sSrc.pos, sDst.pos};
        }
        Finish finish(char* aDst, size_t aDstLen) override
        {
            ZSTD_inBuffer  sSrc{nullptr, 0, 0};
            ZSTD_outBuffer sDst{aDst, aDstLen, 0};
            check("decompressStream/finish", ZSTD_decompressStream(m_State, &sDst, &sSrc));
            return {sDst.pos, sDst.pos < sDst.size};
        }
        virtual ~ReadZstd() { ZSTD_freeDStream(m_State); }
    };

    class WriteZstd : public IFilter
    {
        ZSTD_CStream* m_State = nullptr;

        void check(const char* aMsg, size_t aCode)
        {
            if (ZSTD_isError(aCode))
                throw std::runtime_error("WriteZstd: fail to " + std::string(aMsg) + ": " + std::string(ZSTD_getErrorName(aCode)));
        }

    public:
        WriteZstd(int aLevel = 3)
        {
            Util::Raii sGuard([this]() { ZSTD_freeCStream(m_State); });
            m_State = ZSTD_createCStream();
            if (!m_State)
                throw std::runtime_error("ReadZstd: fail to createCStream");
            check("initCStream", ZSTD_initCStream(m_State, aLevel));
            sGuard.dismiss();
        }
        Pair filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) override
        {
            ZSTD_inBuffer  sSrc{aSrc, aSrcLen, 0};
            ZSTD_outBuffer sDst{aDst, aDstLen, 0};
            check("compressStream", ZSTD_compressStream(m_State, &sDst, &sSrc));
            return {sSrc.pos, sDst.pos};
        }
        Finish finish(char* aDst, size_t aDstLen) override
        {
            ZSTD_outBuffer sDst{aDst, aDstLen, 0};
            check("endStream", ZSTD_endStream(m_State, &sDst));
            return {sDst.pos, sDst.pos < sDst.size};
        }
        virtual ~WriteZstd() { ZSTD_freeCStream(m_State); }
    };
} // namespace Archive
