#pragma once

#define BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/stacktrace.hpp>
#include <parser/Parser.hpp>
#include <parser/Atoi.hpp>

namespace Sentry
{
    namespace bs = boost::stacktrace;
    using Stacktrace = bs::stacktrace;
    Stacktrace GetStacktrace() { return bs::stacktrace(); }

    // ugly hack to make it faster
    // output to stream, and parse
    template<class T>
    void ParseStacktrace(const Stacktrace& aTrace, T aHandler)
    {
        struct Frame {
            std::string function;
            std::string filename;
            int line = 0;
            std::string addr;
        };
        auto assign = [](auto& to, auto&& from){ to.assign(from.data(), from.size()); };

        std::stringstream sBuf;
        sBuf << aTrace;
        const std::string sTmp = sBuf.str();
        std::vector<boost::string_ref> sList;

        Parse::simple(sTmp, sList, '\n');
        for (auto iter = sList.rbegin(); iter != sList.rend() and iter+1 != sList.rend(); iter++)
        {
            auto& x = *iter;
            Frame sFrame;

            size_t sMethodStart = x.find('#');
            if (sMethodStart == std::string::npos)  continue;
            x.remove_prefix(sMethodStart + 2);

            size_t sMethodEnd = x.find(" at ");
            if (sMethodEnd != std::string::npos)    // method name
            {
                const auto sMethod = x.substr(0, sMethodEnd);
                assign(sFrame.function, sMethod);
                x.remove_prefix(sMethodEnd + 4);

                size_t sNameEnd = x.rfind(':');
                if (sNameEnd != std::string::npos)
                {
                    assign(sFrame.filename, x.substr(0, sNameEnd));
                    sFrame.line = Parser::Atoi<int>(x.substr(sNameEnd + 1));
                } else {
                    assign(sFrame.filename, x);
                }

                aHandler(sFrame);
                continue;
            }
            sMethodEnd = x.find(" in ");
            if (sMethodEnd != std::string::npos)    // module name
            {
                const auto sAddr = x.substr(0, sMethodEnd);
                assign(sFrame.addr, sAddr);
                x.remove_prefix(sMethodEnd + 4);

                assign(sFrame.filename, x);
                aHandler(sFrame);
            }
        };
    }
} // namespace Util