#pragma once

//#define BACKWARD_HAS_BFD 1 - broken since binutils 2.33
#define BACKWARD_HAS_DW 1
#include "backward.hpp"

namespace Sentry
{
    using Trace = backward::StackTrace;
    inline Trace GetTrace()
    {
        Trace st;
        st.load_here();
        return st;
    }

    template<class T>
    void ParseTrace(const Trace& aTrace, T aHandler, const size_t aOffset)
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
}
