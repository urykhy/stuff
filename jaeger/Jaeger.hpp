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
        uint64_t traceIdHigh = 0;
        uint64_t traceIdLow  = 0;
        uint64_t parentId    = 0;

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
            const uint64_t sHigh = htobe64(traceIdHigh);
            const uint64_t sLow  = htobe64(traceIdLow);
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
                        sNew.traceIdHigh = be64toh(Parser::from_hex<uint64_t>(aParam.substr(0, 16)));
                        sNew.traceIdLow  = be64toh(Parser::from_hex<uint64_t>(aParam.substr(16)));
                        break;
                    case 3: // parent id
                        sNew.parentId = be64toh(Parser::from_hex<uint64_t>(aParam));
                        break;
                    case 4: // flags not used
                        break;
                    }
                },
                '-');
            return sNew;
        }

        static Params uuid()
        {
            Params sNew;
            std::tie(sNew.traceIdHigh, sNew.traceIdLow) = Util::Uuid64();
            return sNew;
        }
    };

    namespace common = opentelemetry::proto::common::v1;
    namespace trace  = opentelemetry::proto::trace::v1;

    struct Tag
    {
        std::string name;

        std::variant<std::string, const char*, double, bool, int64_t> value;

        void convert(common::KeyValue* aTag) const
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

    class QueueFace : boost::noncopyable
    {
    public:
        virtual void send(trace::ScopeSpans& aMsg) noexcept = 0;
        virtual ~QueueFace(){};
    };
    using QueuePtr = std::shared_ptr<QueueFace>;

    class Store : boost::noncopyable
    {
        const Params      m_Params;
        QueuePtr          m_Queue;
        std::mutex        m_Mutex;
        trace::ScopeSpans m_Spans;

    public:
        Store(const Params& aParams, QueuePtr aQueue)
        : m_Params(aParams)
        , m_Queue(aQueue)
        {
        }

        const Params& params() const
        {
            return m_Params;
        }

        trace::Span* create_span()
        {
            std::unique_lock sLock(m_Mutex);
            return m_Spans.add_spans();
        }

        ~Store()
        {
            m_Queue->send(m_Spans);
        }
    };
    using StorePtr = std::shared_ptr<Store>;

    class Span : boost::noncopyable
    {
        const int m_XCount;
        StorePtr  m_Store;

        trace::Span* m_Span = nullptr;

        const uint64_t m_SpanId = Util::random8();
        bool           m_Alive  = true;

        static StorePtr make_trace(const Params& aParams, QueuePtr aQueue)
        {
            return std::make_shared<Store>(aParams, aQueue);
        }

        static std::string to_binary(uint64_t aVal)
        {
            uint64_t sVal = htobe64(aVal);
            return std::string(reinterpret_cast<const char*>(&sVal), sizeof(sVal));
        }

        void init(const std::string& aName, uint64_t aParentSpanId)
        {
            m_Span = m_Store->create_span();
            m_Span->set_name(aName);
            m_Span->set_trace_id(m_Store->params().binary_trace_id());
            m_Span->set_span_id(to_binary(m_SpanId));
            if (aParentSpanId or m_Store->params().parentId) {
                m_Span->set_parent_span_id(to_binary(
                    aParentSpanId == 0 ? m_Store->params().parentId
                                       : aParentSpanId));
            }
            m_Span->set_start_time_unix_nano(Time::get_time().to_ns());
        }

    public:
        Span(const Params& aParams, QueuePtr aQueue, const std::string& aName)
        : m_XCount(std::uncaught_exceptions())
        , m_Store(make_trace(aParams, aQueue))
        {
            init(aName, 0);
        }

        Span(StorePtr aStore, const std::string& aName, uint64_t aParentSpanId)
        : m_XCount(std::uncaught_exceptions())
        , m_Store(aStore)
        {
            init(aName, aParentSpanId);
        }

        Span(Span&& aOld)
        : m_XCount(aOld.m_XCount)
        , m_Store(aOld.m_Store)
        , m_Span(std::move(aOld.m_Span))
        , m_SpanId(aOld.m_SpanId)
        , m_Alive(aOld.m_Alive)
        {
            aOld.m_Alive = false;
        }

        ~Span()
        {
            close();
        }

        void close() noexcept
        {
            try {
                if (m_Alive) {
                    if (m_XCount != std::uncaught_exceptions())
                        set_error();
                    m_Span->set_end_time_unix_nano(Time::get_time().to_ns());
                    m_Alive = false;
                }
            } catch (...) {
                m_Alive = false;
            }
        }

        Span child(const std::string& aName) const
        {
            return Span(m_Store, aName, m_SpanId);
        }

        void set_tag(const Tag& aTag)
        {
            aTag.convert(m_Span->add_attributes());
        }

        void set_error(const char* aMessage = nullptr)
        {
            auto sStatus = m_Span->mutable_status();
            sStatus->set_message(aMessage != nullptr ? aMessage : "error");
            sStatus->set_code(trace::Status::STATUS_CODE_ERROR);
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

        std::string traceparent() const
        {
            return Params{m_Store->params().traceIdHigh, m_Store->params().traceIdLow, m_SpanId}.traceparent();
        }
    };
    using SpanPtr = std::shared_ptr<Span>;

} // namespace Jaeger
