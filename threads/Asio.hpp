#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>

#include "Group.hpp"

namespace Threads {
    namespace asio = boost::asio;
    class Asio
    {
        asio::io_service m_Service;
        using Guard = boost::asio::executor_work_guard<boost::asio::io_service::executor_type>;
        std::unique_ptr<Guard> m_Guard;

    public:
        void start(Group& tg, size_t n = 1)
        {
            m_Guard = std::make_unique<Guard>(service().get_executor());
            tg.start(
                [this] {
                    threadName("asio");
                    m_Service.run();
                },
                n);
            tg.at_stop([this]() { stop(); });
        }

        void stop()
        {
            m_Guard.reset();
            m_Service.stop();
        }

        ~Asio()
        {
            stop();
        }

        void reset()
        {
            m_Guard.reset();
            m_Service.reset();
        }

        void insert(std::function<void(void)> f)
        {
            service().post(f);
        }

        void spawn(std::function<void(boost::asio::yield_context)>&& aFunc)
        {
            asio::spawn(service(), std::move(aFunc));
        }

        void spawn(std::function<boost::asio::awaitable<void>(void)>&& aFunc)
        {
            asio::co_spawn(service(), std::move(aFunc), boost::asio::detached);
        }

        asio::io_service& service()
        {
            return m_Service;
        }
    };
} // namespace Threads
