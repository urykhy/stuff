#pragma once

#include <optional>

#include "Jaeger.hpp"

#include <unsorted/Log4cxx.hpp>

namespace Jaeger {

    inline SpanPtr start(const Params& aParams, QueuePtr aQueue, const std::string& aName)
    {
        return std::make_shared<Span>(aParams, aQueue, aName);
    }

    inline SpanPtr start(const std::string& aTraceParent, QueuePtr aQueue, const std::string& aName)
    {
        if (!aTraceParent.empty()) {
            return start(Params::parse(aTraceParent), aQueue, aName);
        } else {
            return {};
        }
    }

    inline SpanPtr start(const SpanPtr& aSpan, const std::string& aName)
    {
        if (aSpan) {
            return std::make_shared<Span>(aSpan->child(aName));
        } else {
            return {};
        }
    }

    inline void stop(SpanPtr& aSpan)
    {
        if (aSpan) {
            aSpan->close();
        }
    }

    inline void set_error(SpanPtr& aSpan, const char* aMsg)
    {
        if (aSpan) {
            aSpan->set_error(aMsg);
        }
    }

    inline void set_tag(SpanPtr& aSpan, const Tag& aTag)
    {
        if (aSpan) {
            aSpan->set_tag(aTag);
        }
    }

    template <class... T>
    void set_log(SpanPtr& aSpan, const char* aName, const T&... aTag)
    {
        if (aSpan) {
            aSpan->set_log(aName, aTag...);
        }
    }

} // namespace Jaeger