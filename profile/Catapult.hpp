#pragma once

/*
 * chrome dev tools compatible tracing
 *   to view json: F12, Performance, Load profile
 */

#define FILE_NO_ARCHIVE

#include <stdint.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <boost/noncopyable.hpp>

#include <file/File.hpp>
#include <format/Json.hpp>
#include <time/Meter.hpp>
#include <unsorted/Env.hpp>

namespace Profile::Catapult {

    class Manager;

    struct Event
    {
        char        ph  = 0;
        pid_t       pid = 0;
        pid_t       tid = 0;
        uint64_t    ts  = 0; // time in us
        std::string cat;
        std::string name;

        std::optional<uint64_t>    duration = {};
        std::optional<uint64_t>    counter  = {};
        std::optional<std::string> async    = {};
        std::optional<bool>        instant  = {};

        Format::Json::Value to_json() const
        {
            using namespace Format::Json;
            Value sValue(::Json::objectValue);
            sValue["pid"]  = to_value(pid);
            sValue["tid"]  = to_value(tid);
            sValue["ts"]   = to_value(ts);
            sValue["cat"]  = to_value(cat);
            sValue["name"] = to_value(name);
            if (duration) {
                sValue["ph"]  = "X";
                sValue["dur"] = to_value(duration.value());
            } else if (counter) {
                sValue["ph"] = "C";
                Value sTmp(::Json::objectValue);
                sTmp["value"]  = to_value(counter.value());
                sValue["args"] = std::move(sTmp);
            } else if (async) {
                sValue["ph"] = to_value(std::string_view(&ph, 1));
                sValue["id"] = to_value(async.value());
            } else if (instant) {
                sValue["ph"] = "I";
                sValue["s"]  = "g";
            }
            return sValue;
        }
    };

    class Holder : boost::noncopyable
    {
        friend class Manager;
        Manager* m_Parent = nullptr;
        Event    m_Duration;
        Holder(Manager* aParent, Event&& aDuration);

    public:
        Holder(Holder&& aFrom)
        : m_Parent(aFrom.m_Parent)
        , m_Duration(std::move(aFrom.m_Duration))
        {
            aFrom.m_Parent = nullptr;
        }

        ~Holder();
    };

    class Manager : boost::noncopyable
    {
        std::atomic_bool m_Enabled{false};
        std::string      m_Filename;
        pid_t            m_Pid       = getpid();
        uint64_t         m_StartTime = Time::get_time().to_us();

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        std::vector<Event>           m_Events;
        std::map<pid_t, std::string> m_ThreadNames;

        Event base(const std::string& aCat, const std::string& aName) const
        {
            if (!m_Enabled)
                return {};
            return {.pid  = m_Pid,
                    .tid  = gettid(),
                    .ts   = Time::get_time().to_us() - m_StartTime,
                    .cat  = aCat,
                    .name = aName};
        }

    public:
        Manager(const std::string& aFilename = Util::getEnv("CATAPULT_PROFILE"))
        {
            if (!aFilename.empty()) {
                m_Filename = aFilename;
                m_Enabled  = true;
                m_Events.reserve(1024);
            }
        }

        // counter
        void counter(const std::string& aCat, const std::string& aName, uint64_t aValue)
        {
            if (!m_Enabled)
                return;

            auto sEvent    = base(aCat, aName);
            sEvent.counter = aValue;
            Lock sLock(m_Mutex);
            m_Events.push_back(std::move(sEvent));
        }

        // duration
        Holder start(const std::string& aCat, const std::string& aName)
        {
            if (!m_Enabled)
                return {nullptr, {}};

            return {this, base(aCat, aName)};
        }

        void duration(Event&& aEvent)
        {
            if (!m_Enabled)
                return;

            aEvent.duration = Time::get_time().to_us() - m_StartTime - aEvent.ts;
            Lock sLock(m_Mutex);
            m_Events.push_back(std::move(aEvent));
        }

        // async.
        /*
        void async_start(const std::string& aCat, const std::string& aName, const std::string& aId)
        {
            if (!m_Enabled)
                return;

            auto sEvent  = base(aCat, aName);
            sEvent.ph    = 'b';
            sEvent.async = aId;
            Lock sLock(m_Mutex);
            m_Events.push_back(std::move(sEvent));
        }

        void async_stop(const std::string& aCat, const std::string& aName, const std::string& aId)
        {
            if (!m_Enabled)
                return;

            auto sEvent  = base(aCat, aName);
            sEvent.ph    = 'e';
            sEvent.async = aId;
            Lock sLock(m_Mutex);
            m_Events.push_back(std::move(sEvent));
        }
*/
        // instant
        void instant(const std::string& aCat, const std::string& aName)
        {
            if (!m_Enabled)
                return;

            auto sEvent    = base(aCat, aName);
            sEvent.instant = true;
            Lock sLock(m_Mutex);
            m_Events.push_back(std::move(sEvent));
        }

        void meta(const std::string& aThreadName)
        {
            Lock sLock(m_Mutex);
            m_ThreadNames[gettid()] = aThreadName;
        }

        void dump() const
        {
            if (!m_Enabled)
                return;

            Lock sLock(m_Mutex);
            File::write(
                m_Filename, [this](File::IWriter* aWriter) {
                    using namespace Format::Json;

                    Value sJson(::Json::objectValue);
                    write(sJson, "traceEvents", m_Events);

                    for (auto& [sTid, sName] : m_ThreadNames) {
                        Value sMeta(::Json::objectValue);
                        sMeta["pid"]  = to_value(m_Pid);
                        sMeta["tid"]  = to_value(sTid);
                        sMeta["ts"]   = to_value(0);
                        sMeta["ph"]   = to_value("M");
                        sMeta["cat"]  = to_value("__metadata");
                        sMeta["name"] = to_value("thread_name");
                        Value sArgs(::Json::objectValue);
                        sArgs["name"] = to_value(sName);
                        sMeta["args"] = sArgs;
                        sJson["traceEvents"].append(sMeta);
                    }

                    sJson["beginningOfTime"]  = m_StartTime;
                    const std::string sResult = to_string(sJson, false);
                    aWriter->write(sResult.data(), sResult.size());
                },
                O_TRUNC);
        }
    };

    inline Holder::Holder(Manager* aParent, Event&& aDuration)
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

} // namespace Profile::Catapult