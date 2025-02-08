#pragma once

#include <chrono>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/noncopyable.hpp>

#include "Consumer.hpp"
#include "Producer.hpp"

#include <threads/Coro.hpp>
#include <unsorted/Log4cxx.hpp>

namespace Kafka::Coro {

    struct Meta
    {
        rd_kafka_msg_status_t status{};
        rd_kafka_resp_err_t   error{};
        int32_t               partition = {};
        int64_t               offset    = {};
    };

    class Engine : public boost::noncopyable
    {
        std::atomic_bool m_Stop{false};

    protected:
        using Event = std::unique_ptr<rd_kafka_event_t, void (*)(rd_kafka_event_t*)>;

        virtual void Handle(Event& aEvent) = 0;

        boost::asio::awaitable<void> Coro(QueuePtr aQueue)
        {
            using namespace std::chrono_literals;
            using namespace boost::asio::experimental::awaitable_operators;
            auto sExecutor = co_await boost::asio::this_coro::executor;

            boost::asio::readable_pipe sRead(sExecutor);
            boost::asio::writable_pipe sWrite(sExecutor);
            boost::asio::connect_pipe(sRead, sWrite);

            aQueue->io_event_enable(sWrite.native_handle());
            boost::asio::steady_timer sTimer(sExecutor);
            while (!m_Stop) {
                sTimer.expires_from_now(100ms);
                co_await (
                    sRead.async_read_some(boost::asio::null_buffers(), boost::asio::use_awaitable) ||
                    sTimer.async_wait(boost::asio::use_awaitable));
                while (!m_Stop) {
                    Event sEvent{aQueue->consume(), rd_kafka_event_destroy};
                    if (!sEvent) {
                        break;
                    }
                    Handle(sEvent);
                }
            }
            co_return;
        }

    public:
        void stop()
        {
            m_Stop = true;
        }

        virtual ~Engine() {}
    };

    class Consumer : public Engine, public std::enable_shared_from_this<Consumer>
    {
    public:
        using Callback = std::function<void(const rd_kafka_message_t*)>;

    private:
        Kafka::Consumer m_Consumer;
        QueuePtr        m_Queue;
        Callback        m_Callback;

        struct CommitResponse
        {
            Threads::Coro::Waiter waiter;
            rd_kafka_resp_err_t   error{};
        };

        void Handle(Event& aEvent) override
        {
            const auto sType  = rd_kafka_event_type(aEvent.get());
            const auto sCount = rd_kafka_event_message_count(aEvent.get());
            if (sType == RD_KAFKA_EVENT_FETCH) {
                for (size_t i = 0; i < sCount; i++) {
                    auto sMessage = rd_kafka_event_message_next(aEvent.get());
                    if (!sMessage) {
                        break;
                    }
                    if (sMessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) {
                        DEBUG("got message: "
                              << "topic: " << rd_kafka_topic_name(sMessage->rkt)
                              << ", partition: " << sMessage->partition
                              << ", offset: " << sMessage->offset
                              << ", key:" << Help::key(sMessage));
                    } else {
                        DEBUG("got error: " << rd_kafka_err2name(sMessage->err));
                    }
                    m_Callback(sMessage);
                }
            } else if (sType == RD_KAFKA_EVENT_OFFSET_COMMIT) {
                DEBUG("commited");
                auto sCommitResponse   = static_cast<CommitResponse*>(rd_kafka_event_opaque(aEvent.get()));
                sCommitResponse->error = rd_kafka_event_error(aEvent.get());
                sCommitResponse->waiter.notify();
            }
        }

        static Options patchOptions(Options& aOptions)
        {
            aOptions[":conf_set_events"] = std::to_string(
                RD_KAFKA_EVENT_FETCH | RD_KAFKA_EVENT_OFFSET_COMMIT);
            return aOptions;
        }

    public:
        Consumer(Options&& aOptions, const std::string& aTopic, Callback&& aCallback)
        : m_Consumer(patchOptions(aOptions), aTopic)
        , m_Queue(m_Consumer.queue())
        , m_Callback(std::move(aCallback))
        {
        }

        boost::asio::awaitable<void> start()
        {
            boost::asio::co_spawn(
                co_await boost::asio::this_coro::executor,
                [self = shared_from_this()]() -> boost::asio::awaitable<void> {
                    DEBUG("started consumer coro");
                    co_await self->Coro(self->m_Queue);
                    DEBUG("finished consumer coro");
                },
                boost::asio::detached);
        }

        boost::asio::awaitable<rd_kafka_resp_err_t> sync(const std::string& aTopic, int32_t aPartition, int64_t aOffset)
        {
            CommitResponse sResponse;
            m_Consumer.sync(m_Queue, &sResponse, aTopic, aPartition, aOffset);
            co_await sResponse.waiter.wait(co_await boost::asio::this_coro::executor);
            co_return sResponse.error;
        }
    };

    class Producer : public Engine, public std::enable_shared_from_this<Producer>
    {
        Kafka::Producer m_Producer;

        struct Response
        {
            Threads::Coro::Waiter waiter;
            Meta                  meta;
        };

        void Handle(Event& aEvent) override
        {
            const auto sType  = rd_kafka_event_type(aEvent.get());
            const auto sCount = rd_kafka_event_message_count(aEvent.get());
            if (sType == RD_KAFKA_EVENT_DR) {
                for (size_t i = 0; i < sCount; i++) {
                    auto sMessage = rd_kafka_event_message_next(aEvent.get());
                    if (!sMessage) {
                        break;
                    }
                    void* sOpaque = sMessage->_private;
                    if (sOpaque) {
                        Response* sResponse       = static_cast<Response*>(sOpaque);
                        sResponse->meta.status    = rd_kafka_message_status(sMessage);
                        sResponse->meta.error     = sMessage->err;
                        sResponse->meta.offset    = sMessage->offset;
                        sResponse->meta.partition = sMessage->partition;
                        sResponse->waiter.notify();
                    }
                }
            }
        }

        static Options
        patchOptions(Options& aOptions)
        {
            aOptions[":conf_set_events"] = std::to_string(RD_KAFKA_EVENT_DR);
            return aOptions;
        }

    public:
        Producer(Options&& aOptions, const std::string& aTopic)
        : m_Producer(patchOptions(aOptions), aTopic)
        {
        }

        boost::asio::awaitable<void> start()
        {
            boost::asio::co_spawn(
                co_await boost::asio::this_coro::executor,
                [self = shared_from_this()]() -> boost::asio::awaitable<void> {
                    DEBUG("started producer coro");
                    co_await self->Coro(self->m_Producer.queue());
                    DEBUG("finished producer coro");
                },
                boost::asio::detached);
        }

        boost::asio::awaitable<Meta>
        push(int32_t                aPartition,
             const std::string_view aKey, const std::string_view aValue,
             const Kafka::Producer::Headers& aHeaders = {})
        {
            Response sResponse;
            m_Producer.push(aPartition, aKey, aValue, aHeaders, &sResponse /* opaque */, false /* no poll */);
            co_await sResponse.waiter.wait(co_await boost::asio::this_coro::executor);
            co_return sResponse.meta;
        }
    };

} // namespace Kafka::Coro
