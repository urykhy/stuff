#pragma once

#include <zstd.h>

#include "Interface.hpp"

#include <unsorted/Raii.hpp>

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
        struct Params
        {
            int  threads       = 1;
            bool long_matching = false;

            static Params get_default() { return {}; }
        };

        WriteZstd(int aLevel = 3, const Params& aParams = Params::get_default())
        {
            Util::Raii sGuard([this]() { ZSTD_freeCStream(m_State); });
            m_State = ZSTD_createCStream();
            if (!m_State)
                throw std::runtime_error("ReadZstd: fail to createCStream");
            check("initCStream", ZSTD_initCStream(m_State, aLevel));
            check("ZSTD_c_checksumFlag", ZSTD_CCtx_setParameter(m_State, ZSTD_c_checksumFlag, 1));
            if (aParams.threads > 1)
                check("set ZSTD_c_nbWorkers", ZSTD_CCtx_setParameter(m_State, ZSTD_c_nbWorkers, aParams.threads));
            if (aParams.long_matching)
                check("set ZSTD_c_enableLongDistanceMatching", ZSTD_CCtx_setParameter(m_State, ZSTD_c_enableLongDistanceMatching, aParams.long_matching));
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
