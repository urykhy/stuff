#include <rapidxml/rapidxml_print.hpp>

namespace format::XML {

    using Node = rapidxml::xml_node<>;

    inline std::string to_string(const Node* aXML, bool aIndent = true)
    {
        std::string sStr;
        rapidxml::print(std::back_inserter(sStr), *aXML, 0);
        return sStr;
    }

    inline Node* create_object(Node* aNode, const char* aName)
    {
        auto sNode = aNode->document()->allocate_node(rapidxml::node_element, aName);
        aNode->append_node(sNode);
        return sNode;
    }

    inline void write(Node* aNode, const char* aName, const std::string& aValue)
    {
        char* sPtr  = aNode->document()->allocate_string(aValue.c_str());
        auto  sNode = aNode->document()->allocate_node(rapidxml::node_element, aName, sPtr);
        aNode->append_node(sNode);
    }

} // namespace format::XML