#pragma once

#include <list>
#include <mutex>
#include <set>
#include <string>

namespace Prometheus {

    // normal metric
    struct MetricFace
    {
        const std::string m_Name;

        MetricFace(const std::string& aName);
        virtual std::string format() const = 0;
        virtual ~MetricFace();

    private:
        MetricFace(const MetricFace&) = delete;
        MetricFace& operator=(const MetricFace&) = delete;
    };

    // complex metric. update called to refresh child state
    struct ComplexFace
    {
        ComplexFace();
        virtual void update() = 0;
        virtual ~ComplexFace();

    private:
        ComplexFace(const ComplexFace&) = delete;
        ComplexFace& operator=(const ComplexFace&) = delete;
    };

    // metrics storage
    class Manager
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex     m_Mutex;
        std::set<MetricFace*>  m_Set;
        std::set<ComplexFace*> m_Complex;

        Manager(){};

        friend class MetricFace;
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

        friend class ComplexFace;
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

    public:
        static Manager& instance()
        {
            static Manager sManager;
            return sManager;
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

    inline MetricFace::MetricFace(const std::string& aName)
    : m_Name(aName)
    {
        Manager::instance().MetricInsert(this);
    }
    inline MetricFace::~MetricFace() { Manager::instance().MetricErase(this); }

    inline ComplexFace::ComplexFace() { Manager::instance().ComplexInsert(this); }
    inline ComplexFace::~ComplexFace() { Manager::instance().ComplexErase(this); }
} // namespace Prometheus
