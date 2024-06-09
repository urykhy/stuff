#pragma once

#include <chrono>
#include <string>

#include <sw/redis++/redis++.h>
#include <unsorted/Env.hpp>

namespace Cache::Redis {
    using namespace std::chrono_literals;

    struct Config
    {
        sw::redis::ConnectionOptions options;
        std::string                  prefix = "test:";
        std::chrono::milliseconds    ttl    = 10000ms;

        Config()
        {
            options.host            = Util::getEnv("REDIS_HOST");
            options.port            = 6379;
            options.connect_timeout = std::chrono::milliseconds(100);
            options.socket_timeout  = std::chrono::milliseconds(100);
        }
    };

    class Manager
    {
        const Config     m_Config;
        sw::redis::Redis m_Redis;

    public:
        Manager(const Config& aConfig)
        : m_Config(aConfig)
        , m_Redis(aConfig.options)
        {
        }

        sw::redis::OptionalString get(const std::string& aKey)
        {
            return m_Redis.get(m_Config.prefix + aKey);
        }

        std::vector<sw::redis::OptionalString> mget(const std::vector<std::string>& aKeys)
        {
            std::vector<std::string> sKey;
            for (auto& x : aKeys)
                sKey.push_back(m_Config.prefix + x);
            std::vector<sw::redis::OptionalString> sResult;
            m_Redis.mget(sKey.begin(), sKey.end(), std::back_inserter(sResult));
            return sResult;
        }

        void set(const std::string& aKey, const std::string& aValue)
        {
            m_Redis.set(m_Config.prefix + aKey, aValue, m_Config.ttl);
        }

        void expire(const std::string& aKey)
        {
            m_Redis.pexpire(m_Config.prefix + aKey, m_Config.ttl);
        }
    };
} // namespace Cache::Redis
