#pragma once

#include <list>
#include <sstream>
#include <string>

namespace MySQL
{
    template<class P, class T>
    std::list<std::string> Format(const T& aData)
    {
        std::list<std::string> sList;
        std::stringstream sBuf;

        size_t sCounter = 0;
        for (const auto& x : aData)
        {
            if (sCounter == 0)
                sBuf << "INSERT INTO " << P::table() << " (" << P::fields() << ") VALUES ";
            if (sCounter > 0)
                sBuf << ", ";
            sBuf << '(';
            P::format(sBuf, x);
            sBuf << ')';

            sCounter++;
            if (sCounter >= P::max_size())
            {
                sBuf << ' ' << P::finalize();
                sList.push_back(std::move(sBuf.str()));
                sBuf.str("");
                sCounter = 0;
            }
        }

        if (sCounter)
        {
            sBuf << ' ' << P::finalize();
            sList.push_back(std::move(sBuf.str()));
        }

        return sList;
    }
}
