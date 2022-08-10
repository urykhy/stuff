#pragma once

#include <variant>

#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include "jaeger_proto/Agent.h"
#include "jaeger_proto/jaeger_types.h"
#pragma GCC diagnostic pop

#include <format/Hex.hpp>
#include <mpl/Mpl.hpp>
#include <networking/Resolve.hpp>
#include <networking/UdpSocket.hpp>
#include <parser/Hex.hpp>
#include <parser/Parser.hpp>
#include <parser/Url.hpp>
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
                   Format::to_hex(htobe64(traceIdHigh)) +
                   Format::to_hex(htobe64(traceIdLow)) + '-' +
                   Format::to_hex(htobe64(parentId)) + '-' +
                   "00"; // flags
        };

        static Params parse(std::string_view aParent, std::string_view aState = {})
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
                        sNew.traceIdHigh = be64toh(Parser::from_hex<int64_t>(aParam.substr(0, 16)));
                        sNew.traceIdLow  = be64toh(Parser::from_hex<int64_t>(aParam.substr(16)));
                        break;
                    case 3: // parent id
                        sNew.parentId = be64toh(Parser::from_hex<int64_t>(aParam));
                        break;
                    case 4: // flags not used
                        break;
                    }
                },
                '-');
            Parser::http_header_kv(aState, [&sNew](auto aName, auto aValue) {
                if (aName == "span_offset")
                    sNew.baseId |= int64_t(Parser::from_hex<int8_t>(aValue)) << 56;
            });
            return sNew;
        }

        static Params parse(std::string_view aParent, std::string_view aState, int64_t aBaseId, std::string_view aService)
        {
            auto sParams = parse(aParent, aState);
            sParams.baseId &= 0xFF00000000000000; // preserving high byte (offset)
            sParams.baseId |= aBaseId;            // apply new base id
            sParams.service = aService;
            return sParams;
        }

        static Params uuid(const std::string& aService)
        {
            Params sNew;
            std::tie(sNew.traceIdHigh, sNew.traceIdLow) = Util::Uuid64();
            sNew.service                                = aService;
            return sNew;
        }
    };

    struct Tag
    {
        std::string name;

        std::variant<std::string, const char*, double, bool, int64_t> value;

        jaegertracing::thrift::Tag convert() const
        {
            using Type = jaegertracing::thrift::TagType;
            jaegertracing::thrift::Tag sTag;
            sTag.key = name;
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
                       value);
            return sTag;
        }
    };

    struct Span;
    struct Trace : boost::noncopyable
    {
        friend class Span;

    private:
        jaegertracing::thrift::Batch m_Batch;
        const Params                 m_Params;
        size_t                       m_Counter = 0;

        size_t nextSpanID() { return m_Params.baseId + m_Counter++; }

    public:
        Trace(const Params& aParams)
        : m_Params(aParams)
        {
            m_Batch.process.serviceName = aParams.service;
        }

        void set_process_tag(const Tag& aTag)
        {
            m_Batch.process.tags.push_back(aTag.convert());
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
    };

    class Span : boost::noncopyable
    {
        const int                   m_XCount;
        Trace&                      m_Trace;
        jaegertracing::thrift::Span m_Span;
        bool                        m_Alive = true;

    public:
        Span(Trace& aTrace, const std::string& aName, size_t aParentSpanId = 0)
        : m_XCount(std::uncaught_exceptions())
        , m_Trace(aTrace)
        {
            m_Span.traceIdHigh = m_Trace.m_Params.traceIdHigh;
            m_Span.traceIdLow  = m_Trace.m_Params.traceIdLow;
            m_Span.spanId      = m_Trace.nextSpanID();
            if (aParentSpanId == 0)
                m_Span.parentSpanId = m_Trace.m_Params.parentId;
            else
                m_Span.parentSpanId = aParentSpanId;
            m_Span.operationName = aName;
            m_Span.flags         = 0;
            m_Span.startTime     = Time::get_time().to_us();
            m_Span.duration      = 0;

            // BOOST_TEST_MESSAGE("new span " << aName << "(" << m_Span.spanId << ") with parent " << m_Span.parentSpanId);
        }

        Span(Span&& aOld)
        : m_XCount(aOld.m_XCount)
        , m_Trace(aOld.m_Trace)
        , m_Span(std::move(aOld.m_Span))
        , m_Alive(aOld.m_Alive)
        {
            aOld.m_Alive = false;
        }

        ~Span()
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
                m_Trace.m_Batch.spans.push_back(std::move(m_Span));
                m_Alive = false;
            }
        }

        Span child(const std::string& aName)
        {
            return Span(m_Trace, aName, m_Span.spanId);
        }

        void set_tag(const Tag& aTag)
        {
            m_Span.tags.push_back(aTag.convert());
            m_Span.__isset.tags = true;
        }

        void set_error() { set_tag(Tag{"error", true}); }

        template <class... T>
        void set_log(const T&... aTag)
        {
            jaegertracing::thrift::Log sLog;
            sLog.timestamp = Time::get_time().to_us();

            // convert every aTag
            (static_cast<void>(sLog.fields.push_back(aTag.convert())), ...);

            m_Span.logs.push_back(sLog);
            m_Span.__isset.logs = true;
        }

        std::string trace_id() const
        {
            return Format::to_hex(htobe64(m_Span.traceIdHigh)) +
                   Format::to_hex(htobe64(m_Span.traceIdLow));
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

    inline void send(const Trace& aTrace)
    {
        static Udp::Socket sUdp(Util::resolveName(Util::getEnv("JAEGER_HOST")),
                                Util::getEnv<uint16_t>("JAEGER_PORT", 6831));
        const std::string  sMessage = aTrace.serialize();
        sUdp.write(sMessage.data(), sMessage.size());
    }
} // namespace Jaeger
