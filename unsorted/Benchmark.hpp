#pragma once

#include <benchmark/benchmark.h>

#include <chrono>

#include <boost/asio/steady_timer.hpp>

#include <prometheus/Histogramm.hpp>
#include <time/Meter.hpp>

namespace Benchmark {
    template <class I, class H, class F>
    inline void Coro(benchmark::State& state, unsigned aCount, I aInit, H aHandler, F aFini)
    {
        boost::asio::io_service sAsio;
        bool                    sExit    = false;
        uint32_t                sCount   = 0;
        uint32_t                sRunning = 0;
        uint32_t                sError   = 0;
        Prometheus::Histogramm  sLatency;

        boost::asio::co_spawn(
            sAsio,
            [&]() -> boost::asio::awaitable<void> {
                using namespace std::chrono_literals;
                boost::asio::steady_timer sTimer(sAsio);
                for (auto _ : state) {
                    sTimer.expires_from_now(1ms);
                    co_await sTimer.async_wait(boost::asio::use_awaitable);
                }
                sExit = true;
            },
            boost::asio::detached);

        for (unsigned i = 0; i < aCount; i++) {
            boost::asio::co_spawn(
                sAsio,
                [&, aSerial = i]() -> boost::asio::awaitable<void> {
                    sRunning++;
                    if (sRunning == 1) {
                        co_await aInit();
                    }
                    while (!sExit) {
                        Time::Meter sMeter;
                        try {
                            co_await aHandler(aSerial);
                            sLatency.tick(sMeter.get().to_ms());
                            sCount++;
                        } catch (...) {
                            sError++;
                        }
                    }
                    sRunning--;
                    if (!sRunning) {
                        co_await aFini();
                    }
                },
                boost::asio::detached);
        }

        Time::Meter sMeter;
        sAsio.run();
        const double                    sELA = sMeter.get().to_double();
        constexpr std::array<double, 4> sProb{0.5, 0.99, 1.0};
        const auto                      sResult = sLatency.quantile(sProb);
        state.counters["rps"]                   = sCount / sELA;
        state.counters["err"]                   = sError / sELA;
        state.counters["lat(0.50)"]             = sResult[0];
        state.counters["lat(0.99)"]             = sResult[1];
        state.counters["lat(1.00)"]             = sResult[2];
    }

    class RateLimit
    {
        using Timer = boost::asio::steady_timer;

        const unsigned         m_Tick; // time to one request in us
        unsigned               m_Count = 0;
        Time::Meter            m_Meter;
        std::unique_ptr<Timer> m_Timer;

    public:
        RateLimit(unsigned aRPS)
        : m_Tick(1'000'000 / (double)aRPS)
        {
            if (aRPS > 1'000'000) {
                throw std::invalid_argument("RateLimit: too high rps");
            }
        }

        template <class T>
        boost::asio::awaitable<void> operator()(T&& aHandler)
        {
            std::exception_ptr sPtr;
            m_Count++;
            try {
                co_await aHandler();
            } catch (...) {
                sPtr = std::current_exception();
            }
            const auto sELA = m_Meter.get().to_us();
            if (sELA < m_Count * m_Tick) {
                if (!m_Timer) {
                    m_Timer = std::make_unique<Timer>(co_await boost::asio::this_coro::executor);
                }
                m_Timer->expires_from_now(std::chrono::microseconds(m_Tick));
                co_await m_Timer->async_wait(boost::asio::use_awaitable);
            }
            if (sELA >= 1'000'000) {
                m_Count = 0;
                m_Meter.reset();
            }
            if (sPtr) {
                std::rethrow_exception(sPtr);
            }
        }
    };

} // namespace Benchmark
