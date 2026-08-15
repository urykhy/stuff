#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include <glaze/glaze.hpp>
#pragma GCC diagnostic pop

#include <fmt/core.h>

#include <chrono>
#include <iostream>
#include <thread>

#include <unsorted/Interning.hpp>
#include <unsorted/Random.hpp>
#include <unsorted/Tcc.hpp>

namespace Expr {

    using UserParams = std::map<std::string, glz::json_t, std::less<>>;

    struct OpCache
    {
        std::vector<uint8_t> m_Cache;

        OpCache(uint32_t aSize = 1024)
        : m_Cache(aSize / 8 + ((aSize % 8) > 0 ? 1 : 0))
        {
        }
        static uint32_t Byte(uint32_t aKey)
        {
            return aKey / 8;
        }
        static uint8_t Bit(uint32_t aKey)
        {
            return 1 << (aKey % 8);
        }
        bool Get(uint32_t aKey) const
        {
            assert(Byte(aKey) < m_Cache.size());
            return m_Cache[Byte(aKey)] & Bit(aKey);
        }
        void Set(uint32_t aKey)
        {
            assert(Byte(aKey) < m_Cache.size());
            m_Cache[Byte(aKey)] |= Bit(aKey);
        }
    };

    struct UserData
    {
        Util::Interning                 m_Dict;
        UserParams                      m_Params;
        std::vector<std::optional<int>> m_IntCache;
        OpCache                         m_OpCache;

        void Prepare()
        {
            for (auto& i : m_Params) {
                for (const auto& x : i.second.get_object()) {
                    m_Dict(i.first + "." + x.first); // source.param_name
                }
            }
            m_IntCache.resize(m_Dict.size());
        }
    };

    class Compiler
    {
        Util::TinyCompiler m_Tcc;
        using Method    = int (*)(void*);
        Method m_Method = nullptr;

        static std::pair<std::string_view, std::string_view> Split(const std::string_view aStr)
        {
            size_t sPos = aStr.find('.');
            if (sPos != std::string::npos) {
                return {aStr.substr(0, sPos), aStr.substr(sPos + 1)};
            }
            return {aStr, ""};
        }

    public:
        static int
        tccGetInt(void* aPtr, int aIntern, const char* aName)
        {
            UserData* sData  = static_cast<UserData*>(aPtr);
            auto&     sEntry = sData->m_IntCache[aIntern];

            if (!sEntry.has_value()) {
                // search source
                auto [sSource, sParam] = Split(aName);
                auto sSourceIt         = sData->m_Params.find(sSource);
                if (sSourceIt != sData->m_Params.end()) {
                    // search param in source
                    const auto& sObject = sSourceIt->second.get_object();
                    if (auto sParamIt = sObject.find(sParam); sParamIt != sObject.end()) {
                        // return sParamIt->second.as<int>();
                        sEntry = sParamIt->second.as<int>();
                    } else {
                        sEntry = 0;
                    }
                } else {
                    sEntry = 0;
                }
            }
            return sEntry.value();
        }

        static int tccIsCached(void* aPtr, int aKey)
        {
            UserData* sData = static_cast<UserData*>(aPtr);
            return sData->m_OpCache.Get(aKey * 2);
        }

        static int tccReadCache(void* aPtr, int aKey)
        {
            UserData* sData = static_cast<UserData*>(aPtr);
            return sData->m_OpCache.Get(aKey * 2 + 1);
        }

        static int tccStoreCache(void* aPtr, int aKey, int aValue)
        {
            UserData* sData = static_cast<UserData*>(aPtr);
            sData->m_OpCache.Set(aKey * 2);
            if (aValue > 0) {
                sData->m_OpCache.Set(aKey * 2 + 1);
            }
            return aValue;
        }

        static int delay()
        {
            using namespace std::chrono_literals;
            const auto start  = std::chrono::high_resolution_clock::now();
            const auto finish = start + std::chrono::nanoseconds(100);
            while (std::chrono::high_resolution_clock::now() < finish) {
                _mm_pause();
            }
            return 1;
        }

        void Compile(const std::string& aCode, const std::string& aMethod)
        {
            m_Tcc.AddSymbol("getIntValue", tccGetInt);
            m_Tcc.AddSymbol("isCached", tccIsCached);
            m_Tcc.AddSymbol("readCache", tccReadCache);
            m_Tcc.AddSymbol("storeCache", tccStoreCache);
            m_Tcc.AddSymbol("delay", delay);
            m_Tcc.Compile(aCode);
            m_Method = (Method)m_Tcc.GetSymbol(aMethod);
            if (!m_Method) {
                throw std::invalid_argument("fail to get symbol: " + aMethod);
            }
        }

        int Call(UserData& aData)
        {
            return m_Method(static_cast<void*>(&aData));
        }
    };

} // namespace Expr
