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
                        co_await aHandler(aSerial);
                        sLatency.tick(sMeter.get().to_ms());
                        sCount++;
                    }
                    sRunning--;
                    if (!sRunning) {
                        co_await aFini();
                    }
                },
                boost::asio::detached);
        }

        sAsio.run();
        state.SetItemsProcessed(sCount);
        constexpr std::array<double, 4> sProb{0.5, 0.9, 0.99, 1.0};
        const auto                      sResult = sLatency.quantile(sProb);

        state.counters["latency(q0.50)"] = sResult[0];
        state.counters["latency(q0.90)"] = sResult[1];
        state.counters["latency(q0.99)"] = sResult[2];
        state.counters["latency(q1.00)"] = sResult[3];
    }
} // namespace Benchmark