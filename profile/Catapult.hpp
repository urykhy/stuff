#pragma once

/*
 * chrome dev tools compatible tracing
 *   to view json: F12, Performance, Load profile
 */

#ifdef CATAPULT_PROFILE

#define FILE_NO_ARCHIVE

#include <stdint.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

#include <boost/noncopyable.hpp>

#include <container/Collect.hpp>
#include <file/File.hpp>
#include <format/Json.hpp>
#include <mpl/Mpl.hpp>
#include <threads/SafeQueue.hpp>
#include <time/Meter.hpp>
#include <unsorted/Env.hpp>

namespace Profile::Catapult {

    class PerThread;
    class Manager;

    inline Manager*                g_Manager   = nullptr;
    inline thread_local PerThread* g_PerThread = nullptr;

    struct Meta
    {
        pid_t pid = getpid();
        pid_t tid = gettid();

        void to_json(Format::Json::Value& aValue) const
        {
            using namespace Format::Json;
            aValue["pid"] = to_value(pid);
            aValue["tid"] = to_value(tid);
        }
    };

    struct Event
    {
        std::string cat;
        std::string name;
        uint64_t    ts = 0; // time in us

        struct None
        {
        };
        struct Duration
        {
            uint64_t value = 0;
        };
        struct Counter
        {
            uint64_t value = 0;
        };
        struct Mark
        {
        };
        using Variant   = std::variant<None, Duration, Counter, Mark>;
        Variant variant = {};

        void to_json(Format::Json::Value& aValue) const
        {
            using namespace Format::Json;
            aValue["ts"]   = to_value(ts);
            aValue["cat"]  = to_value(cat);
            aValue["name"] = to_value(name);

            std::visit(
                Mpl::overloaded{
                    [&](const None&) {
                        throw std::logic_error("event with empty variant");
                    },
                    [&](const Duration& aDuration) {
                        aValue["ph"]  = "X";
                        aValue["dur"] = to_value(aDuration.value);
                    },
                    [&](const Counter& aCounter) {
                        aValue["ph"] = "C";
                        Value sTmp(::Json::objectValue);
                        sTmp["value"]  = to_value(aCounter.value);
                        aValue["args"] = std::move(sTmp);
                    },
                    [&](const Mark&) {
                        aValue["ph"] = "I";
                        aValue["s"]  = "g";
                    }},
                variant);
        }
    };

    class Holder : boost::noncopyable
    {
        PerThread* m_Parent = nullptr;
        Event      m_Duration;

    public:
        Holder()
        {}

        Holder(PerThread* aParent, Event&& aDuration);

        Holder(Holder&& aFrom)
        : m_Parent(aFrom.m_Parent)
        , m_Duration(std::move(aFrom.m_Duration))
        {
            aFrom.m_Parent = nullptr;
        }

        ~Holder();
    };

    class PerThread : boost::noncopyable
    {
        using Queue = Container::Collect<Event>;
        const uint64_t m_StartTime;
        const Meta     m_Meta;
        Queue          m_Queue;

        Event make(const std::string& aCat, const std::string& aName) const
        {
            return {.cat  = aCat,
                    .name = aName,
                    .ts   = Time::get_time().to_us() - m_StartTime};
        }

    public:
        PerThread(uint64_t aStartTime); // need Manager declaration

        // counter
        void counter(const std::string& aCat, const std::string& aName, uint64_t aValue)
        {
            auto sEvent    = make(aCat, aName);
            sEvent.variant = Event::Counter{aValue};
            m_Queue.insert(std::move(sEvent));
        }

        // duration
        Holder start(const std::string& aCat, const std::string& aName)
        {
            return {this, make(aCat, aName)};
        }

        void duration(Event&& aEvent)
        {
            aEvent.variant = Event::Duration{Time::get_time().to_us() - m_StartTime - aEvent.ts};
            m_Queue.insert(std::move(aEvent));
        }

        // instant event
        void mark(const std::string& aCat, const std::string& aName)
        {
            auto sEvent    = make(aCat, aName);
            sEvent.variant = Event::Mark{};
            m_Queue.insert(std::move(sEvent));
        }

        void flush()
        {
            m_Queue.flush();
        }
    };

    class Manager : boost::noncopyable
    {
        std::atomic_bool m_Enabled{false};
        const uint64_t   m_StartTime = Time::get_time().to_us();

        struct Info
        {
            Meta               meta;
            std::vector<Event> events;
        };

        using Lock  = std::unique_lock<std::mutex>;
        using Queue = Threads::SafeQueueThread<Info>;

        mutable std::mutex m_Mutex;
        struct ThreadInfo
        {
            std::unique_ptr<PerThread> engine;
            std::string                name;
        };
        std::map<pid_t, ThreadInfo> m_Threads;

        Queue m_Queue;

        std::unique_ptr<File::FileWriter> m_File;
        std::unique_ptr<File::BufWriter>  m_Writer;
        bool                              m_Comma = false;

        void write(Format::Json::Value& aJson)
        {
            using namespace Format::Json;

            if (!m_Comma)
                m_Comma = true;
            else
                m_Writer->write(",\n", 2);
            const std::string sTmp = to_string(aJson, false);
            m_Writer->write(sTmp.data(), sTmp.size());
        }

        void process(Info& aInfo)
        {
            using namespace Format::Json;

            for (auto& x : aInfo.events) {
                Value sEntry(::Json::objectValue);
                aInfo.meta.to_json(sEntry);
                x.to_json(sEntry);
                write(sEntry);
            }
        }

    public:
        Manager(const std::string& aFilename)
        : m_Queue([this](Info& aInfo) { process(aInfo); })
        {
            if (!aFilename.empty()) {
                m_Enabled = true;
                g_Manager = this;

                m_File   = std::make_unique<File::FileWriter>(aFilename, O_TRUNC);
                m_Writer = std::make_unique<File::BufWriter>(m_File.get());

                std::stringstream sTmp;
                sTmp << R"({"beginningOfTime":)" << m_StartTime << R"(,"traceEvents":[)" << '\n';
                const auto& sStr = sTmp.str();
                m_Writer->write(sStr.data(), sStr.size());
            }
        }

        ~Manager()
        {
            g_Manager = nullptr;
        }

        void start(Threads::Group& aGroup)
        {
            if (!m_Enabled)
                return;

            m_Queue.start(aGroup);
        }

        void thread(const std::string& aThreadName)
        {
            if (!m_Enabled)
                return;

            Lock  sLock(m_Mutex);
            auto& sInfo = m_Threads[gettid()];

            if (!sInfo.name.empty())
                return; // already registered

            sInfo.name   = aThreadName;
            sInfo.engine = std::make_unique<PerThread>(m_StartTime);
            g_PerThread  = sInfo.engine.get();
        }

        // data chunk from per thread collector
        void insert(const Meta& aMeta, std::vector<Event>&& aEvents)
        {
            if (!m_Enabled)
                return;

            m_Queue.insert({aMeta, std::move(aEvents)});
        }

        void done()
        {
            if (!m_Enabled)
                return;

            for (auto& x : m_Threads)
                x.second.engine->flush();

            m_Enabled = false;
            g_Manager = nullptr;

            while (!m_Queue.idle())
                Threads::sleep(0.001);

            // append metadata
            for (auto& [sTid, sInfo] : m_Threads) {
                using namespace Format::Json;
                Value sMeta(::Json::objectValue);
                sMeta["pid"]  = to_value(getpid());
                sMeta["tid"]  = to_value(sTid);
                sMeta["ts"]   = to_value(0);
                sMeta["ph"]   = to_value("M");
                sMeta["cat"]  = to_value("__metadata");
                sMeta["name"] = to_value("thread_name");
                Value sArgs(::Json::objectValue);
                sArgs["name"] = to_value(sInfo.name);
                sMeta["args"] = sArgs;
                write(sMeta);
            }

            std::string_view sEnd = "\n]}";
            m_Writer->write(sEnd.data(), sEnd.size());
            m_Writer->flush();
            m_Writer.reset();
            m_File.reset();
        }
    };

    inline Holder::Holder(PerThread* aParent, Event&& aDuration)
    : m_Parent(aParent)
    , m_Duration(aDuration)
    {}

    inline Holder::~Holder()
    {
        try {
            if (m_Parent)
                m_Parent->duration(std::move(m_Duration));
        } catch (...) {
        }
    }

    inline PerThread::PerThread(uint64_t aStartTime)
    : m_StartTime(aStartTime)
    , m_Queue(1000, [this](std::vector<Event>& aEvents) {
        g_Manager->insert(m_Meta, std::move(aEvents));
    })
    {}

} // namespace Profile::Catapult

#define CATAPULT_MANAGER(x) Profile::Catapult::Manager sCatapultManager(x);

#define CATAPULT_START(x) sCatapultManager.start(x);

#define CATAPULT_THREAD(x)            \
    if (Profile::Catapult::g_Manager) \
        Profile::Catapult::g_Manager->thread(x);

#define CATAPULT_COUNTER(c, n, v)       \
    if (Profile::Catapult::g_PerThread) \
        Profile::Catapult::g_PerThread->counter(c, n, v);

#define CATAPULT_EVENT(c, n)                                    \
    auto sCatapultHolder = []() {                               \
        if (Profile::Catapult::g_PerThread)                     \
            return Profile::Catapult::g_PerThread->start(c, n); \
        else                                                    \
            return Profile::Catapult::Holder();                 \
    }();

#define CATAPULT_MARK(c, n)             \
    if (Profile::Catapult::g_PerThread) \
        Profile::Catapult::g_PerThread->mark(c, n);

#define CATAPULT_DONE() sCatapultManager.done();

#else
#define CATAPULT_MANAGER(x)       ;
#define CATAPULT_START(x)         ;
#define CATAPULT_THREAD(x)        ;
#define CATAPULT_COUNTER(c, n, v) ;
#define CATAPULT_EVENT(c, n)      ;
#define CATAPULT_MARK(c, n)       ;
#define CATAPULT_DONE()           ;
#endif
