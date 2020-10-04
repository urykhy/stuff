#pragma once

#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include "jaeger_types.h"
#include "Agent.h"

#include <variant>
#include <mpl/Mpl.hpp>
#include <time/Meter.hpp>

namespace Jaeger
{
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
        const size_t m_BaseSpanID;
        size_t m_SpanID = 0;
        size_t nextSpanID() { return m_BaseSpanID + m_SpanID++; }

        static jaegertracing::thrift::Tag convert(const Tag& aTag)
        {
            using Type = jaegertracing::thrift::TagType;
            jaegertracing::thrift::Tag sTag;
            sTag.key = aTag.name;
            std::visit(Mpl::overloaded {
                [&sTag](const std::string& arg) { sTag.vType = Type::STRING; sTag.vStr    = arg; sTag.__isset.vStr    = true; },
                [&sTag](const char*        arg) { sTag.vType = Type::STRING; sTag.vStr    = arg; sTag.__isset.vStr    = true; },
                [&sTag](double             arg) { sTag.vType = Type::DOUBLE; sTag.vDouble = arg; sTag.__isset.vDouble = true; },
                [&sTag](bool               arg) { sTag.vType = Type::BOOL;   sTag.vBool   = arg; sTag.__isset.vBool   = true; },
                [&sTag](int64_t            arg) { sTag.vType = Type::LONG;   sTag.vLong   = arg; sTag.__isset.vLong   = true; },
            }, aTag.value);
            return sTag;
        }
    public:

        Metric(const std::string& aName, size_t aBaseID = 0)
        : m_TraceIDHigh(Time::get_time().to_us())
        , m_TraceIDLow(lrand48())
        , m_BaseSpanID(aBaseID)
        {
            m_Batch.process.serviceName = aName;
        }

        Metric(const std::string& aName, const std::tuple<uint64_t, uint64_t>& aUUID, size_t aBaseID = 0)
        : m_TraceIDHigh(std::get<0>(aUUID))
        , m_TraceIDLow(std::get<1>(aUUID))
        , m_BaseSpanID(aBaseID)
        {
            m_Batch.process.serviceName = aName;
        }

        void set_process_tag(const Tag& aTag)
        {
            m_Batch.process.tags.push_back(convert(aTag));
            m_Batch.process.__isset.tags = true;
        }

        std::string serialize() const
        {
            auto sBuffer = std::make_shared<apache::thrift::transport::TMemoryBuffer>();
            auto sProtocol = std::make_shared<apache::thrift::protocol::TCompactProtocol>(sBuffer);
            jaegertracing::agent::thrift::AgentClient sAgent(sProtocol);
            sAgent.emitBatch(m_Batch);
            return sBuffer->getBufferAsString();
        }

        class Guard
        {
            Guard() = delete;
            Guard(const Guard&) = delete;
            Guard& operator=(const Guard&) = delete;

            const int m_XCount;
            Metric&   m_Metric;
            jaegertracing::thrift::Span m_Span;
            bool m_Alive = true;

        public:

            Guard(Metric& aMetric, const std::string& aName, size_t aParent = 0)
            : m_XCount(std::uncaught_exceptions())
            , m_Metric(aMetric)
            {
                m_Span.traceIdHigh   = m_Metric.m_TraceIDHigh;
                m_Span.traceIdLow    = m_Metric.m_TraceIDLow;
                m_Span.spanId        = m_Metric.nextSpanID();
                m_Span.parentSpanId  = aParent;
                m_Span.operationName = aName;
                m_Span.flags         = 0;
                m_Span.startTime     = Time::get_time().to_us();
                m_Span.duration      = 0;
            }

            Guard(Guard&& aOld)
            : m_XCount(aOld.m_XCount)
            , m_Metric(aOld.m_Metric)
            , m_Span(std::move(aOld.m_Span))
            , m_Alive(aOld.m_Alive)
            {
                aOld.m_Alive = false;
            }

            ~Guard() { try { close(); } catch (...) {}; }

            void close()
            {
                if (m_Alive)
                {
                    if (m_XCount != std::uncaught_exceptions())
                        set_error();
                    m_Span.duration = Time::get_time().to_us() - m_Span.startTime;
                    m_Metric.m_Batch.spans.push_back(std::move(m_Span));
                    m_Alive = false;
                }
            }

            uint32_t span_id() const { return m_Span.spanId; }

            Guard child(const std::string& aName)
            {
                return Guard(m_Metric, aName, m_Span.spanId);
            }

            void set_tag(const Tag& aTag)
            {
                m_Span.tags.push_back(Metric::convert(aTag));
                m_Span.__isset.tags = true;
            }

            void set_error() { set_tag(Tag{"error", true}); }

            template<class... T>
            void set_log(const T&... aTag)
            {
                jaegertracing::thrift::Log sLog;
                sLog.timestamp = Time::get_time().to_us();

                // convert every aTag
                (static_cast<void>(sLog.fields.push_back(Metric::convert(aTag))), ...);

                m_Span.logs.push_back(sLog);
                m_Span.__isset.logs = true;
            }
        };
        friend class Guard;
    };

} // namespace Jaeger
