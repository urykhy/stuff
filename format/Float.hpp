#pragma once

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Format {
    inline std::string with_precision(double v, int precision)
    {
        if (!std::isfinite(v))
            return "0";
        std::stringstream s;
        s.setf(std::ios::fixed, std::ios::floatfield);
        s << std::setprecision(precision) << v;
        std::string tmp = s.str();

        while (tmp.size() > 1 and tmp.back() == '0')
            tmp.pop_back();
        if (tmp.size() > 1 and tmp.back() == '.')
            tmp.pop_back();

        return tmp;
    }

    inline std::string for_human(double v)
    {
        if (v > 1E10)
            return with_precision(v / 1E9, 0) + "G";
        if (v > 1E7)
            return with_precision(v / 1E6, 0) + "M";
        if (v > 1E4)
            return with_precision(v / 1E3, 0) + "K";
        if (v > 100)
            return with_precision(v, 0);
        if (v > 10)
            return with_precision(v, 1);
        if (v > 1)
            return with_precision(v, 2);
        if (v > 1E-3)
            return with_precision(v * 1E3, 0) + "m";
        return with_precision(v * 1E6, 0) + "u";
    }
} // namespace Format
