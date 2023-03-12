#pragma once

#include <concepts>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "Atoi.hpp"
#include "Parser.hpp"

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

    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::from_xml)>
    void from_node(const Node* aNode, T& t)
    {
        t.from_xml(aNode);
    }

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

    template <class T>
    void from_path(const Node* aNode, const std::string_view aPath, T& aValue)
    {
        const Node* sCurrent = aNode;
        simple(
            aPath,
            [&sCurrent](std::string_view aName) mutable {
                if (sCurrent != nullptr)
                    sCurrent = sCurrent->first_node(aName.data(), aName.size());
            },
            '.');
        from_node(sCurrent, aValue);
    }

} // namespace Parser::XML

namespace rapidxml {
    template <class T>
    void operator>>(xml_node<>& aNode, T& aValue)
    {
        Parser::XML::from_node(&aNode, aValue);
    }
} // namespace rapidxml