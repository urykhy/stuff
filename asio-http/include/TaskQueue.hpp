
#pragma once

#include <list>
#include <mutex>
#include <Logger.hpp>
#include <boost/asio.hpp>

namespace Util
{
    namespace asio = boost::asio;

    template<class Val>
    class AsioTaskQueue
    {
        boost::asio::io_service& io_service;
        mutable std::mutex mutex;
        using Lock = std::unique_lock<std::mutex>;
        using Handler = std::function<void(Val&&)>;
        std::list<Val> items;
        std::list<Handler> handlers;

    public:
        AsioTaskQueue(asio::io_service& io) : io_service(io) {}

        // worker api
        template<class H>
        void wait(H h) {
            Val item;
            bool run = false;
            {
                Lock lk(mutex);
                if (!items.empty()) {
                    item = std::move(items.front());
                    items.pop_front();
                    run = true;
                } else {
                    handlers.emplace_back(h);
                    TRACE("waiting for query, total " << handlers.size() << " waiters");
                }
            }
            if (run) {
                h(std::move(item));
            }
        }

        // client api
        void push(Val&& item)
        {
            Handler h;
            {
                Lock lk(mutex);
                if (!handlers.empty()) {
                    h = std::move(handlers.front());
                    handlers.pop_front();
                } else {
                    items.emplace_back(std::move(item));
                }
            }
            if (h) {
                io_service.post([v=std::move(item), handler=std::move(h)]() mutable {
                    handler(std::move(v));
                });
            }
        }

        size_t idle() const {
            Lock lk(mutex);
            return handlers.size();
        }

        size_t size() const {
            Lock lk(mutex);
            return items.size();
        }
    };

}
