#pragma once

#include <list>
#include <mutex>
#include <set>
#include <string>

namespace Stat
{
    // normal metric
    struct MetricFace
    {
        // empty name used to hide stat from exporter
        const std::string m_GraphiteName;
        const std::string m_PrometheusName;

        MetricFace(const std::string& aG, const std::string& aP);
        virtual ~MetricFace();
        virtual std::string format() const = 0;
    };

    // complex metric. update called to refresh child state
    struct ComplexFace
    {
        ComplexFace();
        virtual ~ComplexFace();
        virtual void update() = 0;
    };

    // metrics storage
    class Manager
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::set<MetricFace*> m_Set;
        std::set<ComplexFace*> m_Complex;

        Manager() {};
    public:

        static Manager& instance()
        {
            static Manager sManager;
            return sManager;
        }

        void MetricInsert(MetricFace* aMetric) { Lock lk(m_Mutex); m_Set.insert(aMetric); }
        void MetricErase(MetricFace* aMetric)  { Lock lk(m_Mutex); m_Set.erase(aMetric);  }

        void ComplexInsert(ComplexFace* aMetric) { Lock lk(m_Mutex); m_Complex.insert(aMetric); }
        void ComplexErase(ComplexFace* aMetric)  { Lock lk(m_Mutex); m_Complex.erase(aMetric);  }

        // row set without \n
        using Set = std::list<std::string>;

        Set toGraphite(const time_t aMoment = ::time(nullptr)) const
        {
            Lock lk(m_Mutex);
            const std::string sMoment = std::to_string(aMoment);
            Set sSet;
            for (auto x : m_Set)
                if (!x->m_GraphiteName.empty())
                    sSet.push_back(x->m_GraphiteName + ' ' + x->format() + ' ' + sMoment);
            return sSet;
        }

        Set toPrometheus() const
        {
            Lock lk(m_Mutex);
            Set sSet;
            for (auto x : m_Set)
                if (!x->m_PrometheusName.empty())
                    sSet.push_back(x->m_PrometheusName + ' ' + x->format());
            return sSet;
        }

        // refresh complex metrics (Time)
        // call 2 times per interval
        void onTimer()
        {
            Lock lk(m_Mutex);
            for (auto x : m_Complex)
                x->update();
        }
    };

    inline MetricFace::MetricFace(const std::string& aG, const std::string& aP)
    : m_GraphiteName(aG)
    , m_PrometheusName(aP)
    { Manager::instance().MetricInsert(this); }
    inline MetricFace::~MetricFace() { Manager::instance().MetricErase(this); }

    inline ComplexFace::ComplexFace()  { Manager::instance().ComplexInsert(this); }
    inline ComplexFace::~ComplexFace() { Manager::instance().ComplexErase(this); }
}