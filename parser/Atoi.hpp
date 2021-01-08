#pragma once
#include <boost/utility/string_ref.hpp>

namespace Parser
{
    class NotNumber : std::exception
    {
        virtual const char* what() const throw() { return "not a number"; }
    };

    template<class T, class S>
    T Atoi(S&& aString)
    {
        T sResult = 0;
        bool sNegative = false;

        for (auto& x : aString)
        {
            if (x == '-')
            {
                sNegative = true;
                continue;
            }
            if (x > '9' or x < '0')
                throw NotNumber();
            sResult = sResult * 10 + x - '0';
        }

        return sNegative ? -sResult : sResult;
    }

    template<class T, class S>
    T Atoi8(S&& aString)
    {
        T sResult = 0;

        for (auto& x : aString)
        {
            if (x == '\0')
                break;
            if (x > '7' or x < '0')
                throw NotNumber();
            sResult = sResult * 8 + x - '0';
        }

        return sResult;
    }

    template<class T, class S>
    T Atof(S&& aString)
    {
        T sResult = 0;
        int64_t sOrder = 1;
        bool sPoint = false;

        if (!aString.empty() and aString[0]=='-')
        {
            aString.remove_prefix(1);
            sOrder = -1;
        }

        for (auto& x : aString)
        {
            if (x == '.') {
                sPoint = true;
                continue;
            }
            if (x > '9' or x < '0')
                throw NotNumber();
            sResult = sResult * 10 + x - '0';
            if (sPoint)
                sOrder *= 10;
        }

        return sResult / T(sOrder);
    }
}
