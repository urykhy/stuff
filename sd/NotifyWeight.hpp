#pragma once

#include "Entry.hpp"
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
        const Params                m_Params;
        std::shared_ptr<NotifyFace> m_Notify;
        using Info = Util::EwmaRps::Info;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        Util::EwmaRps      m_Ewma;
        double             m_LastWeight   = 0;
        double             m_LastRPS      = 0;
        time_t             m_LastUpdateTs = 0;

        Info info_i() const
        {
            auto sInfo    = m_Ewma.estimate();
            sInfo.latency = std::clamp(m_Ewma.estimate().latency, m_Params.latency_min, m_Params.latency_max);
            sInfo.rps     = std::clamp(m_Ewma.estimate().rps, m_Params.threads / m_Params.latency_max, m_Params.threads / m_Params.latency_min);
            return sInfo;
        }

        std::string format_i()
        {
            const auto [sLatency, sRPS, sSuccessRate] = info_i();
            Entry sEntry;
            sEntry.latency  = sLatency;
            sEntry.rps      = sRPS;
            sEntry.threads  = m_Params.threads;
            sEntry.location = m_Params.location;
            return Format::Json::to_string(sEntry.to_json(), false /* indent */);
        }

        static int relative(double a, double b)
        {
            const double sChange = std::abs(a - b) / std::max(a, b);
            return std::lround(sChange * 100);
        }

    public:
        NotifyWeight(boost::asio::io_service& aAsio, const Params& aParams, std::shared_ptr<NotifyFace> aNotifier = nullptr)
        : m_Params(aParams)
        , m_Ewma(0.95, m_Params.latency)
        {
            m_LastWeight   = m_Params.threads / info_i().latency;
            m_LastUpdateTs = time(nullptr);
            if (aNotifier) {
                m_Notify = aNotifier;
                m_Notify->update(format_i());
            } else
                m_Notify = std::make_shared<SD::Notify>(aAsio, aParams.notify, format_i());
            m_Notify->start();
        }

        void add(time_t aNow, double aLatency)
        {
            Lock lk(m_Mutex);
            bool sNewSecond = m_Ewma.add(aNow, aLatency, true);
            if (sNewSecond) {
                constexpr int MAX_CHANGE            = 10;
                auto [sLatency, sRPS, sSuccessRate] = info_i();
                const double sWeight                = m_Params.threads / sLatency;
                const int    sRelative              = std::max(
                    relative(m_LastWeight, sWeight),
                    relative(m_LastRPS, sRPS));
                if (sRelative > MAX_CHANGE or
                    (sRelative > 3 and aNow >= m_LastUpdateTs + MAX_CHANGE - sRelative)) {
                    m_Notify->update(format_i());
                    m_LastWeight   = sWeight;
                    m_LastRPS      = sRPS;
                    m_LastUpdateTs = aNow;
                }
            }
        }

        Info info() const
        {
            Lock lk(m_Mutex);
            return info_i();
        }
    };
} // namespace SD
