
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Format {
    std::string with_precision(double v, int precision)
    {
        if (!std::isfinite(v))
            return "0";
        std::stringstream s;
        s.setf(std::ios::fixed, std::ios::floatfield);
        s << std::setprecision(precision) << v;
        return s.str();
    }

    std::string for_human(double v)
    {
        if (v > 1E10)
            return with_precision(v / 1E9, 0) + "G";
        if (v > 1E7)
            return with_precision(v / 1E6, 0) + "M";
        if (v > 1E4)
            return with_precision(v / 1E3, 0) + "K";
        return with_precision(v, 0);
    }
} // namespace Format
