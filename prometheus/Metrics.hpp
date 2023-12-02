#pragma once

#include <time.h>

#include <atomic>

#include "Histogramm.hpp"
#include "Manager.hpp"

#include <mpl/Mpl.hpp>

namespace Prometheus {

    template <class... G>
    inline std::string makeName(std::string aName, G&&... aTags)
    {
        if (aName.empty())
            throw std::invalid_argument("Prometheus: empty metric name");

        if (sizeof...(aTags) > 0) {
            if (aName.back() == '}')
                aName.pop_back();
            else
                aName.push_back('{');
            Mpl::for_each_argument(
                Mpl::overloaded{
                    [&aName](const char* aStr) {
                        if (aName.back() != '{')
                            aName.push_back(',');
                        aName.append(aStr);
                    },
                    [&aName](const std::string& aStr) {
                        if (aName.back() != '{')
                            aName.push_back(',');
                        aName.append(aStr);
                    },
                    [&aName](const std::pair<auto, auto>& aTag) {
                        if (aName.back() != '{')
                            aName.push_back(',');
                        aName.append(aTag.first);
                        aName.append("=\"");
                        aName.append(aTag.second);
                        aName.push_back('"');
                    }},
                aTags...);
            aName.push_back('}');
        }
        return aName;
    }

    template <class T = uint64_t>
    class Counter : public MetricFace
    {
        std::atomic<T> m_Value{0};

    public:
        template <class... G>
        Counter(const std::string& aName, G&&... aTags)
        : MetricFace(makeName(aName, std::forward<G>(aTags)...))
        {
        }

        void tick() { m_Value++; }
        void inc(T v) { m_Value += v; }
        void set(T v) { m_Value = v; }

        std::string format() const override { return std::to_string(m_Value); }
    };

    class Age : public MetricFace
    {
        std::atomic<time_t> m_Value{::time(nullptr)};

    public:
        template <class... G>
        Age(const std::string& aName, G&&... aTags)
        : MetricFace(makeName(aName, std::forward<G>(aTags)...))
        {
        }

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

        Histogramm m_Data1;
        Histogramm m_Data2;
        bool       m_Actual1 = true;

        Counter<double> m_50;
        Counter<double> m_90;
        Counter<double> m_99;
        Counter<double> m_00;

    public:
        template <class... G>
        Time(const std::string& aName, G&&... aTags)
        : m_50(aName, std::make_pair("quantile", "0.5"), std::forward<G>(aTags)...)
        , m_90(aName, std::make_pair("quantile", "0.9"), std::forward<G>(aTags)...)
        , m_99(aName, std::make_pair("quantile", "0.99"), std::forward<G>(aTags)...)
        , m_00(aName, std::make_pair("quantile", "1.0"), std::forward<G>(aTags)...)
        {
        }

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
