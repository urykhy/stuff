#pragma once

#define BACKWARD_HAS_BFD 1
#include "backward.hpp"

namespace Sentry
{
    using Stacktrace = backward::StackTrace;
    inline Stacktrace GetStacktrace()
    {
        Stacktrace st;
        st.load_here();
        return st;
    }

    template<class T>
    void ParseStacktrace(const Stacktrace& aTrace, T aHandler, const size_t aOffset)
    {
        struct Frame
        {
            std::string function;
            std::string filename;
            int line = 0;
            std::string addr;
        };
        backward::TraceResolver sResolver;
        sResolver.load_stacktrace(aTrace);
        for (size_t i = aTrace.size()-1; i > aOffset; i--)
        {
            if ((uintptr_t)aTrace[i].addr == 0xffffffffffffffff)
                continue;
            const auto sTrace = sResolver.resolve(aTrace[i]);
            Frame sFrame;
            std::stringstream sTmp;
            sTmp << std::hex << aTrace[i].addr;
            sFrame.addr = sTmp.str();
            sFrame.function = sTrace.source.function;
            sFrame.filename = sTrace.source.filename;
            sFrame.line = sTrace.source.line;
            aHandler(sFrame);
        }
    }
} // namespace Util