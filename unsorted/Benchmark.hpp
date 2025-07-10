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

    struct GetSetConfig
    {
        unsigned COUNT         = 20;   // num of coroutines
        unsigned MAX_READ_RPS  = 1000; // rps per coro
        unsigned MAX_WRITE_RPS = 1000; // rps per coro
    };

    template <class I, class R, class W, class F>
    inline void GetSet(benchmark::State& state, const GetSetConfig& aConfig, I aInit, R aReadOp, W aWriteOp, F aFini)
    {
        using namespace std::chrono_literals;

        boost::asio::io_service sAsio;
        std::atomic_bool        sExit{false};
        unsigned                sReadCount = 0;
        unsigned                sReadError = 0;
        Prometheus::Histogramm  sReadLatency;

        // INIT
        boost::asio::co_spawn(
            sAsio,
            [&]() -> boost::asio::awaitable<void> { co_await aInit(); },
            boost::asio::detached);
        sAsio.run_one();

        for (unsigned i = 0; i < aConfig.COUNT; i++) {
            boost::asio::co_spawn(
                sAsio,
                [&]() -> boost::asio::awaitable<void> {
                    Benchmark::RateLimit sLimit(aConfig.MAX_READ_RPS);
                    while (!sExit) {
                        try {
                            co_await sLimit([&]() -> boost::asio::awaitable<void> {
                                Time::Meter sMeter;
                                co_await aReadOp();
                                sReadLatency.tick(sMeter.get().to_ms());
                                sReadCount++;
                            });
                        } catch (...) {
                            sReadError++;
                        };
                    }
                },
                boost::asio::detached);
        }

        unsigned               sWriteCount = 0;
        unsigned               sWriteError = 0;
        Prometheus::Histogramm sWriteLatency;
        for (unsigned i = 0; i < aConfig.COUNT; i++) {
            boost::asio::co_spawn(
                sAsio,
                [&]() -> boost::asio::awaitable<void> {
                    Benchmark::RateLimit sLimit(aConfig.MAX_WRITE_RPS);
                    while (!sExit) {
                        try {
                            co_await sLimit([&]() -> boost::asio::awaitable<void> {
                                Time::Meter sMeter;
                                co_await aWriteOp();
                                sWriteLatency.tick(sMeter.get().to_ms());
                                sWriteCount++;
                            });
                        } catch (...) {
                            sWriteError++;
                        };
                    }
                },
                boost::asio::detached);
        }

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
                co_await aFini();
            },
            boost::asio::detached);

        Time::Meter sMeter;
        sAsio.run();
        const double sELA = sMeter.get().to_double();

        constexpr std::array<double, 3> sProb{0.5, 0.99, 1.0};
        {
            auto sResult                  = sReadLatency.quantile(sProb);
            state.counters["r:lat(0.50)"] = sResult[0];
            state.counters["r:lat(0.99)"] = sResult[1];
            state.counters["r:lat(1.00)"] = sResult[2];
            state.counters["r:rps"]       = sReadCount / sELA;
            state.counters["r:err"]       = sReadError / sELA;
            sResult                       = sWriteLatency.quantile(sProb);
            state.counters["w:lat(0.50)"] = sResult[0];
            state.counters["w:lat(0.99)"] = sResult[1];
            state.counters["w:lat(1.00)"] = sResult[2];
            state.counters["w:rps"]       = sWriteCount / sELA;
            state.counters["w:err"]       = sWriteError / sELA;
        }
    }

} // namespace Benchmark
