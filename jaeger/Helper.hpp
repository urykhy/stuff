#pragma once

#include "Jaeger.hpp"

#include <unsorted/Log4cxx.hpp>

namespace Jaeger::Helper {

    inline auto create(const std::string& aParent, std::string_view aService)
    {
        if (!aParent.empty())
            return std::make_unique<Trace>(Params::parse(aParent, aService));
        return std::unique_ptr<Trace>(nullptr);
    }

    inline auto start(std::unique_ptr<Trace>& aTrace, const std::string& aName)
    {
        if (aTrace)
            return std::make_optional(Span(*aTrace, aName));
        else
            return std::optional<Span>();
    }

    inline auto start(std::optional<Span>& aSpan, const std::string& aName)
    {
        if (aSpan)
            return std::make_optional(aSpan->child(aName));
        else
            return std::optional<Span>();
    }

    inline void stop(std::optional<Span>& aSpan)
    {
        if (aSpan)
            aSpan->close();
    }

    inline void set_error(std::optional<Span>& aSpan, const char* aMsg)
    {
        if (aSpan) {
            aSpan->set_error();
            aSpan->set_log(Tag{"exception", aMsg});
        }
    }
} // namespace Jaeger::Helper