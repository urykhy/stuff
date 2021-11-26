#pragma once

#include <json/json.h>

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

    // helpers to call t.from_json for structs
    template <typename T, typename = void>
    struct has_from_json : std::false_type
    {};

    template <typename T>
    struct has_from_json<T, std::void_t<decltype(&T::from_json)>> : std::true_type
    {};

    template <class T>
    constexpr bool has_from_json_v = has_from_json<T>::value;

    template <class T>
    typename std::enable_if<has_from_json_v<T>, void>::type from_value(const Value& aJson, T& t) { t.from_json(aJson); }
    // end helpers

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

    template <class T>
    void from_value(const Value& aJson, std::optional<T>& aValue)
    {
        if (aJson.isNull()) {
            aValue.reset();
        } else {
            aValue.emplace();
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

    // helper to get object fields
    template <class T>
    void from_value(const Value& aJson, const std::string& aName, T& aValue)
    {
        if (!aJson.isObject())
            throw std::invalid_argument("not object value");
        if (aJson.isMember(aName))
            from_value(aJson[aName], aValue);
    }

} // namespace Parser::Json
