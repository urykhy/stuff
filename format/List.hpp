#pragma once

namespace Format
{
    template<class L, class F>
    std::ostream& List(std::ostream& aStream, const L& aList, const F& aFunc, const std::string& aSeparator = ", ")
    {
        bool aSecond = false;
        for (const auto& x : aList)
        {
            if (aSecond)
                aStream << aSeparator;
            aStream << aFunc(x);
            aSecond = true;
        }
        return aStream;
    }

    template<class L>
    std::ostream& List(std::ostream& aStream, const L& aList, const std::string& aSeparator = ", ")
    {
        return List(aStream, aList, [](const auto& x) -> auto { return x; }, aSeparator);
    }
}
