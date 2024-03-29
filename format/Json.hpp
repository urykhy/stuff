#pragma once

#include <json/json.h>

#include <concepts>
#include <optional>

namespace Format::Json {

    using Value = ::Json::Value;

    inline std::string to_string(const Value& aJson, bool aIndent = true)
    {
        ::Json::StreamWriterBuilder sBuilder;
        if (!aIndent)
            sBuilder["indentation"] = "";
        return ::Json::writeString(sBuilder, aJson);
    }

    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::to_json)>
    Value to_value(const T& t) { return t.to_json(); }

    template <class T>
    typename std::enable_if<std::is_signed<T>::value, Value>::type
    to_value(const T aValue)
    {
        return Value::Int64(aValue);
    }

    template <class T>
    typename std::enable_if<std::is_unsigned<T>::value, Value>::type
    to_value(const T aValue)
    {
        return Value::UInt64(aValue);
    }

    inline Value to_value(const double aValue)
    {
        return Value(aValue);
    }

    inline Value to_value(std::string_view aValue)
    {
        return Value(aValue.begin(), aValue.end());
    }

    template <class T>
    inline Value to_value(const std::optional<T>& aValue)
    {
        if (!aValue)
            return Value::null;
        return to_value(aValue.value());
    }

    template <class T>
    inline Value to_value(const std::vector<T>& aValue)
    {
        Value sValue(::Json::arrayValue);
        for (auto& x : aValue)
            sValue.append(to_value(x));
        return sValue;
    }

    // universal 'write' method
    template <class T>
    void write(Value& aJson, const std::string& aName, const T& t)
    {
        aJson[aName] = to_value(t);
    }
} // namespace Format::Json
