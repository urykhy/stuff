#pragma once

#include <string>
#include <tuple>

#include "Json.hpp"

namespace Tmp {
    struct Msg1 : public Introspect::UseJson<Msg1>
    {
        int         aaa = 0;
        double      bbb = 0;
        std::string ccc;

        auto __introspect();
        auto __introspect() const;

        // for tests
        auto as_tuple() const
        {
            return std::tie(aaa, bbb, ccc);
        }
        bool operator==(const Msg1& aOther) const
        {
            return as_tuple() == aOther.as_tuple();
        }
    };
    std::ostream& operator<<(std::ostream& aStream, const Msg1& aMsg)
    {
        aStream << "(" << aMsg.aaa << ", " << aMsg.bbb << ", " << aMsg.ccc << ")";
        return aStream;
    }
} // namespace Tmp

#include "Test.inl" // auto generated
