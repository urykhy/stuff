#pragma once

#include <atomic>
#include <time.h>

#include "Manager.hpp"
#include "Histogramm.hpp"

namespace Stat
{
    template<class T = uint64_t>
    class Counter : public MetricFace
    {
        std::atomic<T> m_Value{0};
    public:

        Counter(const std::string& aGraphiteName, const std::string& aPrometheusName)
        : MetricFace(aGraphiteName, aPrometheusName)
        { }

        void tick() { m_Value ++; }
        void inc(T v) { m_Value += v; }
        void set(T v) { m_Value = v; }

        std::string format() const override { return std::to_string(m_Value); }
    };

    class Age : public MetricFace
    {
        std::atomic<time_t> m_Value {::time(nullptr)};
    public:

        Age(const std::string& aGraphiteName, const std::string& aPrometheusName)
        : MetricFace(aGraphiteName, aPrometheusName)
        { }

        void set(time_t v) { m_Value = v; }

        std::string format() const override
        {
            const time_t sNow = ::time(nullptr);
            return sNow > m_Value ? std::to_string(sNow - m_Value) : "0";
        }
    };

    class Time : public ComplexFace
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        using H = Histogramm<10000>;    // 10K buckets with 10 second max. 1ms accuracy
        H m_Data1;
        H m_Data2;
        bool m_Actual1 = true;

        Counter<float> m_50;
        Counter<float> m_90;
        Counter<float> m_99;
        Counter<float> m_00;

        std::string graphiteName(const std::string& aName, const std::string& aTail) const
        {
            if (aName.empty())
                return aName;
            return aName + '.' + aTail;
        }

        std::string prometheusName(const std::string& aName, const std::string& aTag) const
        {
            if (aName.empty())
                return aName;
            if (aName.back() != '}')
                return aName + "{" + aTag + "}";

            // alreay have tags
            auto sName = aName;
            sName.pop_back();
            return sName + ", " + aTag + "}";
        }

    public:

        Time(const std::string& aGraphiteName, const std::string& aPrometheusName, float aMax = 10)
        : m_Data1(aMax)
        , m_Data2(aMax)
        , m_50(graphiteName(aGraphiteName, "quantile_50"),  prometheusName(aPrometheusName, "quantile=\"0.5\""))
        , m_90(graphiteName(aGraphiteName, "quantile_90"),  prometheusName(aPrometheusName, "quantile=\"0.9\""))
        , m_99(graphiteName(aGraphiteName, "quantile_99"),  prometheusName(aPrometheusName, "quantile=\"0.99\""))
        , m_00(graphiteName(aGraphiteName, "quantile_100"), prometheusName(aPrometheusName, "quantile=\"1.0\""))
        { }

        void account(float v)
        {
            auto sBucket = m_Data1.bucket(v);
            Lock lk(m_Mutex);
            m_Data1.tick(sBucket);
            m_Data2.tick(sBucket);
        }

        void update() override
        {
            const std::array<float, 4> m_Prob{0.5,0.9,0.99,1.0}; // 1.0 must be last one
            Lock lk(m_Mutex);
            H& sActual = m_Actual1 ? m_Data1 : m_Data2;
            sActual.quantile(m_Prob, [this](unsigned aIndex, float aValue){
                switch (aIndex)
                {
                case 0: m_50.set(aValue); break;
                case 1: m_90.set(aValue); break;
                case 2: m_99.set(aValue); break;
                case 3: m_00.set(aValue); break;
                }
            });
            sActual.clear();
            m_Actual1 = !m_Actual1;
        }
    };
}
