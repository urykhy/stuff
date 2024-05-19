#pragma once

#include <json/json.h>

#include <concepts>
#include <list>
#include <memory_resource>
#include <optional>

namespace Parser::Json {
    using Value = ::Json::Value;

    inline Value parse(const std::string& aStr)
    {
        Value                               sJson;
        ::Json::CharReaderBuilder           sBuilder;
        std::unique_ptr<::Json::CharReader> sReader(sBuilder.newCharReader());
        std::string                         sErrors;
        if (!sReader->parse(aStr.data(), aStr.data() + aStr.size(), &sJson, &sErrors))
            throw std::invalid_argument(sErrors);
        return sJson;
    }

    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::from_json)>
    void from_value(const Value& aJson, T& t)
    {
        t.from_json(aJson);
    }

    template <class T>
    typename std::enable_if<std::is_signed<T>::value, void>::type
    from_value(const Value& aJson, T& aValue)
    {
        if (not aJson.isInt64())
            throw std::invalid_argument("not int value");
        aValue = aJson.asInt64();
    }

    template <class T>
    typename std::enable_if<std::is_unsigned<T>::value, void>::type
    from_value(const Value& aJson, T& aValue)
    {
        if (not aJson.isUInt64())
            throw std::invalid_argument("not uint value");
        aValue = aJson.asUInt64();
    }

    template <class T>
    typename std::enable_if<std::is_enum<T>::value, void>::type
    from_value(const Value& aJson, T& aValue)
    {
        if (not aJson.isUInt64())
            throw std::invalid_argument("not uint value");
        aValue = static_cast<T>(aJson.asUInt64());
    }

    inline void from_value(const Value& aJson, double& aValue)
    {
        if (not aJson.isDouble())
            throw std::invalid_argument("not double value");
        aValue = aJson.asDouble();
    }

    inline void from_value(const Value& aJson, std::string& aValue)
    {
        if (not aJson.isString())
            throw std::invalid_argument("not string value");
        aValue = aJson.asString();
    }

    inline void from_value(const Value& aJson, std::pmr::string& aValue)
    {
        if (not aJson.isString())
            throw std::invalid_argument("not string value");
        aValue = aJson.asString();
    }

    template <class T, class... Args>
    void from_value(const Value& aJson, std::optional<T>& aValue, Args... aArgs)
    {
        if (aJson.isNull()) {
            aValue.reset();
        } else {
            aValue.emplace(aArgs...);
            from_value(aJson, aValue.value());
        }
    }

    template <class T>
    void from_value(const Value& aJson, std::vector<T>& aValue)
    {
        if (not aJson.isArray())
            throw std::invalid_argument("not array value");
        aValue.clear();
        aValue.resize(aJson.size());
        for (Value::ArrayIndex i = 0; i != aJson.size(); i++)
            from_value(aJson[i], aValue[i]);
    }

    template <class T, class... Args>
    void from_value(const Value& aJson, std::pmr::list<T>& aValue, Args... aArgs)
    {
        if (not aJson.isArray())
            throw std::invalid_argument("not array value");
        aValue.clear();
        for (Value::ArrayIndex i = 0; i != aJson.size(); i++) {
            aValue.emplace_back(aArgs...);
            from_value(aJson[i], aValue.back());
        }
    }

    // helper to get object fields
    template <class T, class... Args>
    void from_object(const Value& aJson, const std::string& aName, T& aValue, Args... aArgs)
    {
        if (!aJson.isObject())
            throw std::invalid_argument("not object value");
        if (aJson.isMember(aName))
            from_value(aJson[aName], aValue, aArgs...);
    }

} // namespace Parser::Json
