#pragma once

#include "Notify.hpp"

#include <format/Json.hpp>
#include <unsorted/Ewma.hpp>

namespace SD {
    struct NotifyWeight
    {
        struct Params
        {
            Notify::Params notify;
            std::string    location = "unknown";
            uint32_t       threads  = 1;
            double         latency  = 0.1; // initial value, to start with

            double latency_min = 0.000001; // clamp latency
            double latency_max = 1000;
        };

    private:
        const Params            m_Params;
        std::shared_ptr<Notify> m_Notify;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        Util::EwmaTs       m_Latency;
        double             m_LastWeight   = 0;
        time_t             m_LastUpdateTs = 0;

        std::string format_i()
        {
            Format::Json::Value sJson(::Json::objectValue);
            Format::Json::write(sJson, "weight", weight_i());
            Format::Json::write(sJson, "threads", m_Params.threads);
            Format::Json::write(sJson, "location", m_Params.location);
            return Format::Json::to_string(sJson, false /* indent */);
        }

        double weight_i() const
        {
            const double sLatency = std::clamp(m_Latency.estimate(), m_Params.latency_min, m_Params.latency_max);
            return m_Params.threads / sLatency;
        }

    public:
        NotifyWeight(boost::asio::io_service& aAsio, const Params& aParams)
        : m_Params(aParams)
        , m_Latency(0.95, m_Params.latency)
        {
            m_LastWeight   = weight_i();
            m_LastUpdateTs = time(nullptr);
            m_Notify       = std::make_shared<SD::Notify>(aAsio, aParams.notify, format_i());
            m_Notify->start();
        }

        void add(double aLatency, time_t aNow)
        {
            Lock lk(m_Mutex);
            bool sNewSecond = m_Latency.add(aLatency, aNow);
            if (sNewSecond) {
                constexpr int MAX_CHANGE = 10;
                const double  sWeight    = weight_i();
                const double  sChange    = std::abs(m_LastWeight - sWeight) / std::max(m_LastWeight, sWeight);
                const int     sRelative  = std::lround(sChange * 100);
                if (sRelative > MAX_CHANGE or
                    (sRelative > 0 and aNow >= m_LastUpdateTs + MAX_CHANGE - sRelative)) {
                    // BOOST_TEST_MESSAGE("new weight: " << sWeight << ", relative change: " << sRelative << ", step " << aNow - m_LastUpdateTs);
                    m_Notify->update(format_i());
                    m_LastWeight   = sWeight;
                    m_LastUpdateTs = aNow;
                }
            }
        }

        double weight() const
        {
            Lock lk(m_Mutex);
            return weight_i();
        }
    };
} // namespace SD