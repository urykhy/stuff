#pragma once

#include <list>
#include <mutex>
#include <set>
#include <string>

#include <boost/core/noncopyable.hpp>

namespace Prometheus {

    // normal metric
    struct MetricFace : public boost::noncopyable
    {
        const std::string m_Name;

        MetricFace(const std::string& aName);
        virtual std::string format() const = 0;
        virtual ~MetricFace();
    };

    // complex metric. update called to refresh child state
    struct ComplexFace : public boost::noncopyable
    {
        ComplexFace();
        virtual void update() = 0;
        virtual ~ComplexFace();
    };

    // metrics storage
    class Manager : public boost::noncopyable
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex     m_Mutex;
        std::set<MetricFace*>  m_Set;
        std::set<ComplexFace*> m_Complex;

        Manager(){};

        friend struct MetricFace;
        void MetricInsert(MetricFace* aMetric)
        {
            Lock lk(m_Mutex);
            m_Set.insert(aMetric);
        }
        void MetricErase(MetricFace* aMetric)
        {
            Lock lk(m_Mutex);
            m_Set.erase(aMetric);
        }

        friend struct ComplexFace;
        void ComplexInsert(ComplexFace* aMetric)
        {
            Lock lk(m_Mutex);
            m_Complex.insert(aMetric);
        }
        void ComplexErase(ComplexFace* aMetric)
        {
            Lock lk(m_Mutex);
            m_Complex.erase(aMetric);
        }

        static Manager m_Instance;

    public:
        static Manager& instance()
        {
            return m_Instance;
        }

        // row set without \n
        using Set = std::list<std::string>;

        template <class T>
        Set format(T aHandler) const
        {
            Lock lk(m_Mutex);
            Set  sSet;
            for (auto x : m_Set)
                sSet.push_back(aHandler(x));
            return sSet;
        }

        Set toPrometheus() const
        {
            return format([](auto x) { return x->m_Name + ' ' + x->format(); });
        }

        // refresh complex metrics (Time, Common)
        void onTimer()
        {
            Lock lk(m_Mutex);
            for (auto x : m_Complex)
                x->update();
        }
    };

    inline Manager Manager::m_Instance;

    inline MetricFace::MetricFace(const std::string& aName)
    : m_Name(aName)
    {
        Manager::instance().MetricInsert(this);
    }
    inline MetricFace::~MetricFace() { Manager::instance().MetricErase(this); }

    inline ComplexFace::ComplexFace() { Manager::instance().ComplexInsert(this); }
    inline ComplexFace::~ComplexFace() { Manager::instance().ComplexErase(this); }
} // namespace Prometheus
