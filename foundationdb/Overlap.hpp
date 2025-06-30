#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>

#include "Client.hpp"

#include <time/Meter.hpp>

namespace FDB {

    // export VersionTimestamp
    class Overlap : public boost::noncopyable
    {
        struct Info
        {
            Client*              client{nullptr};
            std::atomic_bool     running{true};
            std::atomic_uint64_t version{0};
        };
        std::shared_ptr<Info> m_Info;

        static constexpr int LAG_MS = 500;

    public:
        Overlap(Client& aClient)
        {
            m_Info         = std::make_shared<Info>();
            m_Info->client = &aClient;
        }

        boost::asio::awaitable<void> Start()
        {
            boost::asio::co_spawn(
                co_await boost::asio::this_coro::executor,
                [sInfo = m_Info]() mutable -> boost::asio::awaitable<void> {
                    boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                    while (sInfo->running) {
                        try {
                            Transaction sTxn(*sInfo->client);
                            sTxn.Set("__tmp", "overlap"); // set some key, required for GetVersionTimestamp
                            co_await sTxn.CoCommit();
                            const int64_t sVersion = sTxn.GetVersionTimestamp();
                            if (sInfo->version > 0) {
                                sTimer.expires_from_now(std::chrono::milliseconds(LAG_MS));
                                co_await sTimer.async_wait(boost::asio::use_awaitable);
                            }
                            sInfo->version = sVersion;
                        } catch (const std::exception& e) {
                            // BOOST_TEST_MESSAGE("Overlap: " << e.what());
                            sInfo->version = 0;
                        }
                        sTimer.expires_from_now(std::chrono::milliseconds(LAG_MS));
                        co_await sTimer.async_wait(boost::asio::use_awaitable);
                    }
                },
                boost::asio::detached);
            co_return;
        }

        void Stop()
        {
            m_Info->running = false;
        }

        uint64_t GetVersionTimestamp()
        {
            return m_Info->version;
        }
    };
} // namespace FDB
