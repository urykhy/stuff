#pragma once

#include <exception>

namespace Util {
    class Unwind
    {
        const int m_UncaughtExceptions = std::uncaught_exceptions();

    public:
        // return true if stack unwind
        bool operator()() const { return m_UncaughtExceptions != std::uncaught_exceptions(); }
    };
} // namespace Util