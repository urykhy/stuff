#pragma once

#include <format/Json.hpp>
#include <mpl/Mpl.hpp>
#include <parser/Json.hpp>

namespace Introspect {
    template <class B>
    struct UseJson
    {
        void from_json(const ::Json::Value& aJson)
        {
            Mpl::for_each_element(
                [&aJson](auto&& x) { Parser::Json::from_object(aJson, x.first, x.second); },
                static_cast<B*>(this)->__introspect());
        }
        ::Json::Value to_json() const
        {
            ::Json::Value sJson(::Json::objectValue);
            Mpl::for_each_element(
                [&sJson](auto&& x) { sJson[x.first] = Format::Json::to_value(x.second); },
                static_cast<const B*>(this)->__introspect());
            return sJson;
        }
    };
} // namespace Introspect