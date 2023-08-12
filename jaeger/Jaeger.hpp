#pragma once

#include <variant>

#include <boost/noncopyable.hpp>

#include <format/Hex.hpp>
#include <mpl/Mpl.hpp>
#include <parser/Hex.hpp>
#include <parser/Parser.hpp>
#include <time/Meter.hpp>
#include <trace.pb.h>
#include <unsorted/Random.hpp>
#include <unsorted/Uuid.hpp>

namespace Jaeger {

    struct Params
    {
        int64_t     traceIdHigh = 0;
        int64_t     traceIdLow  = 0;
        int64_t     parentId    = 0;
        std::string service{};

        std::string traceparent() const // for HTTP headers
        {
            return "00-" + // version
                   Format::to_hex(htobe64(traceIdHigh)) +
                   Format::to_hex(htobe64(traceIdLow)) + '-' +
                   Format::to_hex(htobe64(parentId)) + '-' +
                   "00"; // flags
        };

        std::string binary_trace_id() const
        {
            std::string sTmp;
            sTmp.reserve(16);
            const int64_t sHigh = htobe64(traceIdHigh);
            const int64_t sLow  = htobe64(traceIdLow);
            sTmp.append(reinterpret_cast<const char*>(&sHigh), sizeof(sHigh));
            sTmp.append(reinterpret_cast<const char*>(&sLow), sizeof(sLow));
            return sTmp;
        }

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
            return sNew;
        }

        static Params parse(std::string_view aParent, std::string_view aService)
        {
            auto sParams    = parse(aParent);
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

        void convert(opentelemetry::proto::common::v1::KeyValue* aTag) const
        {
            aTag->set_key(name);
            auto sValue = aTag->mutable_value();
            std::visit(Mpl::overloaded{
                           [&](const std::string& arg) { sValue->set_string_value(arg); },
                           [&](const char* arg) { sValue->set_string_value(arg); },
                           [&](double arg) { sValue->set_double_value(arg); },
                           [&](bool arg) { sValue->set_bool_value(arg); },
                           [&](int64_t arg) { sValue->set_int_value(arg); },
                       },
                       value);
        }
    };

    class Span;
    struct Trace : boost::noncopyable
    {
        friend class Span;

    private:
        const Params m_Params;

        opentelemetry::proto::trace::v1::TracesData     m_Traces;
        opentelemetry::proto::trace::v1::ResourceSpans* m_Spans = nullptr;
        opentelemetry::proto::trace::v1::ScopeSpans*    m_Scope = nullptr;

    public:
        Trace(const Params& aParams)
        : m_Params(aParams)
        {
            m_Spans = m_Traces.add_resource_spans();
            m_Scope = m_Spans->add_scope_spans();

            Tag sTag{"service.name", m_Params.service};
            sTag.convert(m_Spans->mutable_resource()->add_attributes());
        }

        void set_process_tag(const Tag& aTag)
        {
            aTag.convert(m_Spans->mutable_resource()->add_attributes());
        }

        std::string to_string() const
        {
            return m_Traces.SerializeAsString();
        }
    };

    class Span : boost::noncopyable
    {
        const int m_XCount;
        Trace&    m_Trace;

        opentelemetry::proto::trace::v1::Span* m_Span = nullptr;

        int64_t m_SpanId = Util::random8();
        bool    m_Alive  = true;

        static std::string to_binary(int64_t aVal)
        {
            int64_t sVal = htobe64(aVal);
            return std::string(reinterpret_cast<const char*>(&sVal), sizeof(sVal));
        }

    public:
        Span(Trace& aTrace, const std::string& aName, int64_t aParentSpanId = 0)
        : m_XCount(std::uncaught_exceptions())
        , m_Trace(aTrace)
        {
            m_Span = m_Trace.m_Scope->add_spans();
            m_Span->set_name(aName);
            m_Span->set_trace_id(m_Trace.m_Params.binary_trace_id());
            m_Span->set_span_id(to_binary(m_SpanId));
            m_Span->set_parent_span_id(to_binary(
                aParentSpanId == 0 ? m_Trace.m_Params.parentId
                                   : aParentSpanId));
            m_Span->set_start_time_unix_nano(Time::get_time().to_ns());
        }

        Span(Span&& aOld)
        : m_XCount(aOld.m_XCount)
        , m_Trace(aOld.m_Trace)
        , m_Span(std::move(aOld.m_Span))
        , m_SpanId(aOld.m_SpanId)
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
                m_Span->set_end_time_unix_nano(Time::get_time().to_ns());
                m_Alive = false;
            }
        }

        Span child(const std::string& aName)
        {
            return Span(m_Trace, aName, m_SpanId);
        }

        void set_tag(const Tag& aTag)
        {
            aTag.convert(m_Span->add_attributes());
        }

        void set_error(const char* aMessage = nullptr)
        {
            auto sStatus = m_Span->mutable_status();
            sStatus->set_message(aMessage != nullptr ? aMessage : "error");
            sStatus->set_code(opentelemetry::proto::trace::v1::Status::STATUS_CODE_ERROR);
        }

        template <class... T>
        void set_log(const char* aName, const T&... aTag)
        {
            auto sLog = m_Span->add_events();
            sLog->set_time_unix_nano(Time::get_time().to_ns());
            sLog->set_name(aName);
            (static_cast<void>(aTag.convert(sLog->add_attributes())), ...);
        }

        std::string trace_id() const // for UI, logging.
        {
            return Format::to_hex(m_Span->trace_id());
        }

        Params extract() const
        {
            return {
                m_Trace.m_Params.traceIdHigh,
                m_Trace.m_Params.traceIdLow,
                m_SpanId};
        }
    };
} // namespace Jaeger
