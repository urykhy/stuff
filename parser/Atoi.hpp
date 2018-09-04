#pragma once
#include <boost/utility/string_ref.hpp>

namespace Parser
{
    class NotNumber : std::exception
    {
        virtual const char* what() const throw() { return "not a number"; }
    };

    template<class T>
    T Atoi(const boost::string_ref aString)
    {
        T sResult = 0;

        for (auto& x : aString)
        {
            if (x > '9' or x < '0')
                throw NotNumber();
            sResult = sResult * 10 + x - '0';
        }

        return sResult;
    }

    template<class T>
    T Atof(boost::string_ref aString)
    {
        T sResult = 0;
        T sOrder = 1;
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
                sOrder /= 10.0;
        }

        return sResult * sOrder;
    }
}
