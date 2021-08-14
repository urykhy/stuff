#pragma once

#include <variant>

#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include "jaeger_proto/Agent.h"
#include "jaeger_proto/jaeger_types.h"

#include <format/Hex.hpp>
#include <mpl/Mpl.hpp>
#include <networking/Resolve.hpp>
#include <networking/UdpSocket.hpp>
#include <parser/Hex.hpp>
#include <parser/Parser.hpp>
#include <time/Meter.hpp>
#include <unsorted/Env.hpp>
#include <unsorted/Uuid.hpp>

namespace Jaeger {
    struct Params
    {
        int64_t     traceIdHigh = 0;
        int64_t     traceIdLow  = 0;
        int64_t     parentId    = 0;
        int64_t     baseId      = 1;
        std::string service{};

        std::string traceparent() const
        {
            return "00-" + // version
                   Format::to_hex(traceIdHigh) +
                   Format::to_hex(traceIdLow) + '-' +
                   Format::to_hex(parentId) + '-' +
                   "00"; // flags
        };

        static Params parse(std::string_view aParent)
        {
            Params sNew;
            Parser::simple(
                aParent,
                [sSerial = 0, &sNew](std::string_view aParam) mutable {
                    sSerial++;
                    switch (sSerial) {
                    case 1: // version
                        if (aParam != "00")
                            throw std::invalid_argument("Jaeger::Params: version not supported");
                        break;
                    case 2: // trace id
                        sNew.traceIdHigh = Parser::from_hex<int64_t>(aParam.substr(0, 16));
                        sNew.traceIdLow  = Parser::from_hex<int64_t>(aParam.substr(16));
                        break;
                    case 3: // parent id
                        sNew.parentId = Parser::from_hex<int64_t>(aParam);
                        break;
                    case 4: // flags not used
                        break;
                    }
                },
                '-');
            return sNew;
        }

        static Params uuid(const std::string& aService)
        {
            Params sNew;
            std::tie(sNew.traceIdHigh, sNew.traceIdLow) = Util::Uuid64();
            sNew.service                                = aService;
            return sNew;
        }
    };

    struct Metric : boost::noncopyable
    {
        struct Tag
        {
            std::string name;

            std::variant<std::string, const char*, double, bool, int64_t> value;
        };

    private:
        jaegertracing::thrift::Batch m_Batch;
        const Params                 m_Params;
        size_t                       m_Counter = 0;

        size_t nextSpanID() { return m_Params.baseId + m_Counter++; }

        static jaegertracing::thrift::Tag convert(const Tag& aTag)
        {
            using Type = jaegertracing::thrift::TagType;
            jaegertracing::thrift::Tag sTag;
            sTag.key = aTag.name;
            std::visit(Mpl::overloaded{
                           [&sTag](const std::string& arg) {
                               sTag.vType = Type::STRING;
                               sTag.__set_vStr(arg);
                           },
                           [&sTag](const char* arg) {
                               sTag.vType = Type::STRING;
                               sTag.__set_vStr(arg);
                           },
                           [&sTag](double arg) {
                               sTag.vType = Type::DOUBLE;
                               sTag.__set_vDouble(arg);
                           },
                           [&sTag](bool arg) {
                               sTag.vType = Type::BOOL;
                               sTag.__set_vBool(arg);
                           },
                           [&sTag](int64_t arg) {
                               sTag.vType = Type::LONG;
                               sTag.__set_vLong(arg);
                           },
                       },
                       aTag.value);
            return sTag;
        }

    public:
        Metric(const Params& aParams)
        : m_Params(aParams)
        {
            m_Batch.process.serviceName = aParams.service;
        }

        void set_process_tag(const Tag& aTag)
        {
            m_Batch.process.tags.push_back(convert(aTag));
            m_Batch.process.__isset.tags = true;
        }

        std::string serialize() const
        {
            auto sBuffer   = std::make_shared<apache::thrift::transport::TMemoryBuffer>();
            auto sProtocol = std::make_shared<apache::thrift::protocol::TCompactProtocol>(sBuffer);

            jaegertracing::agent::thrift::AgentClient sAgent(sProtocol);
            sAgent.emitBatch(m_Batch);
            return sBuffer->getBufferAsString();
        }

        class Step : boost::noncopyable
        {
            const int                   m_XCount;
            Metric&                     m_Metric;
            jaegertracing::thrift::Span m_Span;
            bool                        m_Alive = true;

        public:
            Step(Metric& aMetric, const std::string& aName, size_t aParentSpanId = 0)
            : m_XCount(std::uncaught_exceptions())
            , m_Metric(aMetric)
            {
                m_Span.traceIdHigh = m_Metric.m_Params.traceIdHigh;
                m_Span.traceIdLow  = m_Metric.m_Params.traceIdLow;
                m_Span.spanId      = m_Metric.nextSpanID();
                if (aParentSpanId == 0)
                    m_Span.parentSpanId = m_Metric.m_Params.parentId;
                else
                    m_Span.parentSpanId = aParentSpanId;
                m_Span.operationName = aName;
                m_Span.flags         = 0;
                m_Span.startTime     = Time::get_time().to_us();
                m_Span.duration      = 0;

                //BOOST_TEST_MESSAGE("new span " << aName << "(" << m_Span.spanId << ") with parent " << m_Span.parentSpanId);
            }

            Step(Step&& aOld)
            : m_XCount(aOld.m_XCount)
            , m_Metric(aOld.m_Metric)
            , m_Span(std::move(aOld.m_Span))
            , m_Alive(aOld.m_Alive)
            {
                aOld.m_Alive = false;
            }

            ~Step()
            {
                try {
                    close();
                } catch (...) {
                };
            }

            void close()
            {
                if (m_Alive) {
                    if (m_XCount != std::uncaught_exceptions())
                        set_error();
                    m_Span.duration = Time::get_time().to_us() - m_Span.startTime;
                    m_Metric.m_Batch.spans.push_back(std::move(m_Span));
                    m_Alive = false;
                }
            }

            int64_t span_id() const { return m_Span.spanId; }

            Step child(const std::string& aName)
            {
                return Step(m_Metric, aName, m_Span.spanId);
            }

            void set_tag(const Tag& aTag)
            {
                m_Span.tags.push_back(Metric::convert(aTag));
                m_Span.__isset.tags = true;
            }

            void set_error() { set_tag(Tag{"error", true}); }

            template <class... T>
            void set_log(const T&... aTag)
            {
                jaegertracing::thrift::Log sLog;
                sLog.timestamp = Time::get_time().to_us();

                // convert every aTag
                (static_cast<void>(sLog.fields.push_back(Metric::convert(aTag))), ...);

                m_Span.logs.push_back(sLog);
                m_Span.__isset.logs = true;
            }

            Params extract() const
            {
                return {
                    m_Span.traceIdHigh,
                    m_Span.traceIdLow,
                    m_Span.spanId,
                };
            }
        };
        friend class Step;
    };

    inline void send(const Metric& aMetric)
    {
        static Udp::Socket sUdp(Util::resolveName(Util::getEnv("JAEGER_HOST")),
                                Util::getEnv<uint16_t>("JAEGER_PORT", 6831));
        const std::string  sMessage = aMetric.serialize();
        sUdp.write(sMessage.data(), sMessage.size());
    }
} // namespace Jaeger
