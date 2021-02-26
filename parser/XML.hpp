#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Atoi.hpp"

#include <rapidxml/rapidxml.hpp>

namespace Parser::XML {

    // destructive parsing
    inline auto parse(std::string& aStr)
    {
        auto sXML = std::make_unique<rapidxml::xml_document<>>();
        sXML->parse<0>(aStr.data());
        return sXML;
    }

    using Node = rapidxml::xml_node<>;

    // helpers to call t.from_xml for structs
    template <typename T, typename = void>
    struct has_from_xml : std::false_type
    {};

    template <typename T>
    struct has_from_xml<T, std::void_t<decltype(&T::from_xml)>> : std::true_type
    {};

    template <class T>
    constexpr bool has_from_xml_v = has_from_xml<T>::value;

    template <class T>
    typename std::enable_if<has_from_xml_v<T>, void>::type from_node(const Node* aNode, T& t) { t.from_xml(aNode); }
    // end helpers

    template <class T>
    typename std::enable_if<std::numeric_limits<T>::is_integer, void>::type
    from_node(const Node* aNode, T& aValue)
    {
        if (aNode == nullptr)
            return;
        std::string_view sTmp(aNode->value(), aNode->value_size());
        aValue = Parser::Atoi<T>(sTmp);
    }

    inline void from_node(const Node* aNode, std::string& aValue)
    {
        if (aNode == nullptr)
            return;
        aValue.assign(aNode->value(), aNode->value_size());
    }

    inline void from_node(const Node* aNode, bool& aValue)
    {
        if (aNode == nullptr)
            return;
        std::string_view sTmp(aNode->value(), aNode->value_size());
        aValue = sTmp == "true";
    }

    template <class T>
    void from_node(const Node* aNode, std::vector<T>& aValue)
    {
        if (aNode == nullptr)
            return;
        do {
            aValue.push_back({});
            from_node(aNode, aValue.back());
            aNode = aNode->next_sibling(aNode->name(), aNode->name_size());
        } while (aNode);
    }

} // namespace Parser::XML
