#pragma once

#include <filesystem>
#include <map>
#include <string>

#define FILE_NO_ARCHIVE

#include <file/File.hpp>
#include <string/String.hpp>
#include <unsorted/Env.hpp>
#include <unsorted/Log4cxx.hpp>

namespace Config {

    inline log4cxx::LoggerPtr sLogger = Logger::Get("property");

    class PropertyFile
    {
        using Map = std::map<std::string, std::string>;
        Map m_Params;

        void read_file(const std::string& aFilename)
        {
            INFO("read properties from " << aFilename);
            File::by_string(aFilename, [this](std::string_view aStr) {
                auto [sParam, sValue] = parse(aStr);
                if (!sParam.empty() and !sValue.empty())
                    m_Params[std::string(sParam)] = sValue;
            });
        }

        std::string wild_get(const std::string& aName) const
        {
            size_t sPos = 0;
            while (true) {
                size_t sSep = aName.find_first_of(":.", sPos);
                if (sSep == std::string::npos)
                    break;
                std::string sTmp = aName;
                sTmp.replace(sPos, sSep - sPos, "*");
                DEBUG("try wildcard match " << sTmp);
                auto sIt = m_Params.find(sTmp);
                if (sIt != m_Params.end()) {
                    return sIt->second;
                }
                sPos = sSep + 1;
            }
            return {};
        }

    public:
        PropertyFile(const std::string& aFilename)
        {
            if (aFilename.empty())
                return;
            read_file(aFilename);
        }
        std::string get(const std::string& aName) const
        {
            auto sIt = m_Params.find(aName);
            if (sIt == m_Params.end()) {
                return wild_get(aName);
            }
            return sIt->second;
        }

#ifdef BOOST_TEST_MODULE
        void set(const std::string& aName, const std::string& aValue)
        {
            m_Params[aName] = aValue;
        }

    public:
#endif
        static std::pair<std::string_view, std::string_view> parse(std::string_view aStr)
        {
            String::trim(aStr);
            if (aStr.empty())
                return {};
            if (String::starts_with(aStr, "#"))
                return {};
            std::string_view sParam;
            std::string_view sValue;

            size_t sPos = aStr.find('=');
            if (sPos != std::string_view::npos) {
                sParam = aStr.substr(0, sPos);
                String::trim(sParam);
                sValue = aStr.substr(sPos + 1);
                String::trim(sValue);
            } else
                throw std::invalid_argument("bad property: " + std::string(aStr));

            return std::make_pair(sParam, sValue);
        }
    };

    class Context
    {
        static thread_local std::string m_Name;

    public:
        Context(const std::string& aName)
        {
            if (!m_Name.empty())
                m_Name.push_back('.');
            m_Name.append(aName);
        }

        ~Context()
        {
            const size_t sPos = m_Name.find_last_of('.');
            if (sPos == std::string::npos)
                m_Name.clear();
            else
                m_Name.erase(sPos);
        }

        static const std::string& get()
        {
            return m_Name;
        }
    };

    inline thread_local std::string Context::m_Name;

    class Manager
    {
        PropertyFile m_Properties;

        static std::string detectFileName()
        {
            const std::string sEnv = Util::getEnv("PROPERTIES");
            if (!sEnv.empty())
                return sEnv;
            const std::string sDefault("default.properties");
            if (std::filesystem::exists(sDefault))
                return sDefault;
            return {};
        }

        static std::string envName(const std::string& aParamName)
        {
            std::string sResult = String::replace_all(String::replace_all(aParamName, ":", "__"), ".", "_");
            String::toupper(sResult);
            return sResult;
        }

        std::string get_i(const std::string& aName) const
        {
            std::string_view sCtxName = Context::get();

            while (true) {
                std::string sPropertyName;
                if (!sCtxName.empty())
                    sPropertyName += std::string(sCtxName) + ':';
                sPropertyName.append(aName);
                std::string sEnvName = envName(sPropertyName);

                std::string sEnvVal = Util::getEnv(sEnvName.c_str());
                DEBUG("lookup environment: " << sEnvName << " = " << sEnvVal);
                if (!sEnvVal.empty())
                    return sEnvVal;
                std::string sPropertyVal = m_Properties.get(sPropertyName);
                DEBUG("lookup properties: " << sPropertyName << " = " << sPropertyVal);
                if (!sPropertyVal.empty())
                    return sPropertyVal;

                if (sCtxName.empty())
                    break;

                // trim last element of context name and retry
                size_t sPos = sCtxName.rfind('.');
                if (sPos != std::string_view::npos)
                    sCtxName.remove_suffix(sCtxName.size() - sPos);
                else
                    sCtxName = {};
            }
            return {};
        }

    public:
        Manager(const std::string& aFilename = detectFileName())
        : m_Properties(aFilename)
        {
            INFO("manager created");
        }

        std::string get(const std::string& aName, std::optional<std::string> aDefault = {}) const
        {
            std::string sResult = get_i(aName);
            if (!sResult.empty())
                return sResult;

            if (aDefault.has_value()) {
                DEBUG("property " << aName << " not configured, use default value: " << *aDefault);
                return *aDefault;
            }
            ERROR("required property " << aName << " not found");
            throw std::invalid_argument("required property " + aName + " not found");
        }

#ifdef BOOST_TEST_MODULE
        PropertyFile& properties()
        {
            return m_Properties;
        }
#endif
    };

    // static inline default_config

} // namespace Config