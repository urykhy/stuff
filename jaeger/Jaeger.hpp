#pragma once

#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include "jaeger_types.h"
#include "Agent.h"

#include <variant>
#include <time/Meter.hpp>

namespace Jaeger
{
    // helpers to use variant
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    struct Metric
    {
        struct Tag
        {
            std::string name;
            std::variant<std::string, const char*, double, bool, int64_t> value;
        };

    private:

        jaegertracing::thrift::Batch m_Batch;
        const uint64_t m_TraceIDHigh;
        const uint64_t m_TraceIDLow;
        size_t m_SpanID = 0;

        jaegertracing::thrift::Tag convert(const Tag& aTag)
        {
            using Type = jaegertracing::thrift::TagType;
            jaegertracing::thrift::Tag sTag;
            sTag.key = aTag.name;
            std::visit(overloaded {
                [&sTag](const std::string& arg) { sTag.vType = Type::STRING; sTag.vStr    = arg; sTag.__isset.vStr    = true; },
                [&sTag](const char*        arg) { sTag.vType = Type::STRING; sTag.vStr    = arg; sTag.__isset.vStr    = true; },
                [&sTag](double             arg) { sTag.vType = Type::DOUBLE; sTag.vDouble = arg; sTag.__isset.vDouble = true; },
                [&sTag](bool               arg) { sTag.vType = Type::BOOL;   sTag.vBool   = arg; sTag.__isset.vBool   = true; },
                [&sTag](int64_t            arg) { sTag.vType = Type::LONG;   sTag.vLong   = arg; sTag.__isset.vLong   = true; },
            }, aTag.value);
            return sTag;
        }

    public:

        Metric(const std::string& aName)
        : m_TraceIDHigh(Time::get_time().to_us())
        , m_TraceIDLow(lrand48())
        {
            m_Batch.process.serviceName = aName;
        }

        Metric(const std::string& aName, const std::pair<uint64_t, uint64_t>& aUUID)
        : m_TraceIDHigh(aUUID.first)
        , m_TraceIDLow(aUUID.second)
        {
            m_Batch.process.serviceName = aName;
        }

        void set_process_tag(const Tag& aTag)
        {
            m_Batch.process.tags.push_back(convert(aTag));
            m_Batch.process.__isset.tags = true;
        }

        void set_span_tag(size_t aID, const Tag& aTag)
        {
            m_Batch.spans[aID].tags.push_back(convert(aTag));
            m_Batch.spans[aID].__isset.tags = true;
        }

        void set_span_error(size_t aID)
        {
            set_span_tag(aID, Tag{"error", true});
        }

        template<class... T>
        void span_log(size_t aID, const T&... aTag)
        {
            jaegertracing::thrift::Log sLog;
            sLog.timestamp = Time::get_time().to_us();

            // convert every aTag
            (static_cast<void>(sLog.fields.push_back(convert(aTag))), ...);

            m_Batch.spans[aID].logs.push_back(sLog);
            m_Batch.spans[aID].__isset.logs = true;
        }

        size_t start(const std::string& aName, size_t aParent = 0)
        {
            jaegertracing::thrift::Span sSpan;
            sSpan.traceIdHigh = m_TraceIDHigh;
            sSpan.traceIdLow = m_TraceIDLow;
            sSpan.spanId = m_SpanID++;
            sSpan.parentSpanId = aParent;
            sSpan.operationName = aName;
            sSpan.flags = 0;
            sSpan.startTime = Time::get_time().to_us();
            sSpan.duration = 0;
            m_Batch.spans.push_back(sSpan);
            return sSpan.spanId;
        }

        void stop(size_t aID)
        {
            m_Batch.spans[aID].duration = Time::get_time().to_us() - m_Batch.spans[aID].startTime;
        }

        std::string serialize() const
        {
            auto sBuffer = std::make_shared<apache::thrift::transport::TMemoryBuffer>();
            auto sProtocol = std::make_shared<apache::thrift::protocol::TCompactProtocol>(sBuffer);
            jaegertracing::agent::thrift::AgentClient sAgent(sProtocol);
            sAgent.emitBatch(m_Batch);
            return sBuffer->getBufferAsString();
        }
    };

} // namespace Jaeger