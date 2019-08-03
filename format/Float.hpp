
#include <sstream>
#include <iostream>
#include <iomanip>

namespace Format
{
    std::string with_precision(double v, int precision)
    {
        if (!std::isfinite(v))
            return "0";
        std::stringstream s;
        s.setf(std::ios::fixed, std::ios::floatfield);
        s << std::setprecision(precision) << v;
        return s.str();
    }
}
