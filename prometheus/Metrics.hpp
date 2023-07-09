#pragma once

#include <time.h>

#include <atomic>

#include "Histogramm.hpp"
#include "Manager.hpp"

namespace Prometheus {

    template <class T = uint64_t>
    class Counter : public MetricFace
    {
        std::atomic<T> m_Value{0};

    public:
        Counter(const std::string& aName)
        : MetricFace(aName)
        {}

        void tick() { m_Value++; }
        void inc(T v) { m_Value += v; }
        void set(T v) { m_Value = v; }

        std::string format() const override { return std::to_string(m_Value); }
    };

    class Age : public MetricFace
    {
        std::atomic<time_t> m_Value{::time(nullptr)};

    public:
        Age(const std::string& aName)
        : MetricFace(aName)
        {}

        void set(time_t v) { m_Value = v; }

        std::string format() const override
        {
            const time_t sNow = ::time(nullptr);
            return sNow > m_Value ? std::to_string(sNow - m_Value) : "0";
        }
    };

    inline std::string appendTag(const std::string& aName, const std::string& aTag)
    {
        if (aName.empty() or aName.back() != '}')
            return aName + "{" + aTag + "}";

        // alreay have tags
        auto sName = aName;
        sName.pop_back();
        return sName + ", " + aTag + "}";
    }

    class Time : public ComplexFace
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        Histogramm m_Data1;
        Histogramm m_Data2;
        bool       m_Actual1 = true;

        Counter<double> m_50;
        Counter<double> m_90;
        Counter<double> m_99;
        Counter<double> m_00;

    public:
        Time(const std::string& aName)
        : m_50(appendTag(aName, "quantile=\"0.5\""))
        , m_90(appendTag(aName, "quantile=\"0.9\""))
        , m_99(appendTag(aName, "quantile=\"0.99\""))
        , m_00(appendTag(aName, "quantile=\"1.0\""))
        {}

        void account(double v)
        {
            Lock lk(m_Mutex);
            m_Data1.tick(v);
            m_Data2.tick(v);
        }

        void update() override
        {
            constexpr std::array<double, 4> m_Prob{0.5, 0.9, 0.99, 1.0};

            Histogramm sTmp;
            {
                Lock  lk(m_Mutex);
                auto& sActual = m_Actual1 ? m_Data1 : m_Data2;
                sTmp          = std::move(sActual);
                sActual.clear();
                m_Actual1 = !m_Actual1;
            }

            const auto sResult = sTmp.quantile(m_Prob);
            m_50.set(sResult[0]);
            m_90.set(sResult[1]);
            m_99.set(sResult[2]);
            m_00.set(sResult[3]);
        }
    };
} // namespace Prometheus
