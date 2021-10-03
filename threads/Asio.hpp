#pragma once
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "Group.hpp"

namespace Threads {
    namespace asio = boost::asio;
    class Asio
    {
        asio::io_service io_service;

    public:
        void start(Group& tg, size_t n = 1)
        {
            tg.start(
                [this] {
                    threadName("asio");
                    asio::io_service::work work(io_service);
                    io_service.run();
                },
                n);
            tg.at_stop([this]() { term(); });
        }
        void insert(std::function<void(void)> f)
        {
            io_service.post(f);
        }

        void spawn(std::function<void(boost::asio::yield_context)>&& aFunc)
        {
            boost::asio::spawn(service(), std::move(aFunc));
        }

        void term()
        {
            io_service.stop();
        }
        void reset()
        {
            io_service.reset();
        }
        asio::io_service& service()
        {
            return io_service;
        }
        ~Asio()
        {
            term();
        }
    };
} // namespace Threads
