#pragma once

#include <librdkafka/rdkafkacpp.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/noncopyable.hpp>

namespace Kafka {

    using Options         = std::map<std::string, std::string>;
    constexpr int TIMEOUT = 1000; // 1s in ms

    namespace {

        inline std::string to_string(RdKafka::Error* aErr)
        {
            return aErr->str() + '(' + aErr->name() + ')';
        }

        inline void check(RdKafka::Error* aErr, const std::string& aMsg)
        {
            if (aErr != nullptr) {
                std::unique_ptr<RdKafka::Error> sPtr(aErr);
                std::string                     sErr = "Kafka::" + aMsg + ": " + to_string(aErr);
                throw std::runtime_error(sErr);
            }
        }

        inline void check(RdKafka::ErrorCode aCode, const std::string& aMsg)
        {
            if (aCode != RdKafka::ERR_NO_ERROR) {
                throw std::runtime_error("Kafka::" + aMsg + ": " + RdKafka::err2str(aCode));
            }
        }

        inline void configure(RdKafka::Conf* aKafka, const Options& aOptions)
        {
            std::string sErr;
            for (auto& [key, value] : aOptions) {
                if (aKafka->set(key, value, sErr) != RdKafka::Conf::CONF_OK)
                    throw std::invalid_argument("Kafka: invalid option " + key + ": " + sErr);
            }
        }

        inline void ensure_topic(RdKafka::Handle* aHandle, const std::string& aTopic, const std::string& aMsg)
        {
            std::string                     sErr;
            std::unique_ptr<RdKafka::Topic> sTopic(RdKafka::Topic::create(aHandle, aTopic, nullptr, sErr));
            if (!sTopic)
                throw std::invalid_argument("Kafka::" + aMsg + ": can't create topic handle: " + sErr);

            RdKafka::Metadata* sRawMeta = nullptr;
            check(aHandle->metadata(false, sTopic.get(), &sRawMeta, TIMEOUT), aMsg + ": metadata");
            std::unique_ptr<RdKafka::Metadata> sMeta(sRawMeta);

            auto sTopicsMeta = sMeta->topics();
            if (sTopicsMeta == nullptr)
                throw std::invalid_argument("Kafka::" + aMsg + ": topic not exists: " + sErr);
            for (auto& x : *sTopicsMeta) {
                if (x->err() != RdKafka::ERR_NO_ERROR)
                    throw std::invalid_argument("Kafka::" + aMsg + ": topic not exists: " + RdKafka::err2str(x->err()));
            }
        }
    }; // namespace

    namespace Help {
        using Message = std::unique_ptr<RdKafka::Message>;

        std::string_view key(const Message& aMsg)
        {
            return std::string_view(static_cast<const char*>(aMsg->key_pointer()), aMsg->key_len());
        }

        std::string_view value(const Message& aMsg)
        {
            return std::string_view(static_cast<const char*>(aMsg->payload()), aMsg->len());
        }

        template <class T>
        void headers(const Message& aMsg, T&& aHandler)
        {
            if (aMsg->headers() == nullptr)
                return;
            for (auto& x : aMsg->headers()->get_all())
                aHandler(x.key(),
                         std::string_view((const char*)x.value(), x.value_size()));
        }
    }; // namespace Help

    class Consumer
    {
        std::vector<std::string>                m_Topics;
        std::unique_ptr<RdKafka::Conf>          m_Config;
        std::unique_ptr<RdKafka::KafkaConsumer> m_Consumer;

    public:
        Consumer(const Options&        aOptions,
                 const std::string&    aTopic,
                 RdKafka::RebalanceCb* aRebalance = nullptr,
                 RdKafka::EventCb*     aEvent     = nullptr)
        : m_Config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL))
        {
            std::string sErr;
            configure(m_Config.get(), aOptions);

            if (aRebalance) {
                if (m_Config->set("rebalance_cb", aRebalance, sErr) != RdKafka::Conf::CONF_OK) {
                    throw std::invalid_argument("Kafka::Consumer: can't set rebalance_cb: " + sErr);
                }
            }
            if (aEvent) {
                if (m_Config->set("event_cb", aEvent, sErr) != RdKafka::Conf::CONF_OK) {
                    throw std::invalid_argument("Kafka::Consumer: can't set event_cb: " + sErr);
                }
            }

            m_Consumer.reset(RdKafka::KafkaConsumer::create(m_Config.get(), sErr));
            if (!m_Consumer)
                throw std::invalid_argument("Kafka::Consumer: can't create consumer: " + sErr);

            ensure_topic(m_Consumer.get(), aTopic, "Consumer");

            m_Topics.push_back(aTopic);
            check(m_Consumer->subscribe(m_Topics), "Consumer: subscribe");
        }

        ~Consumer()
        {
            m_Consumer->unsubscribe();
            // wait for empty assignment...
            for (int i = 0; i < 10; i++) {
                try {
                    consume(100);
                    if (position().vector.empty())
                        break;
                } catch (...) {
                }
            }
            m_Consumer->close();
        }

        std::unique_ptr<RdKafka::Message> consume(int aTimeout = TIMEOUT)
        {
            return std::unique_ptr<RdKafka::Message>(m_Consumer->consume(aTimeout));
        }

        auto groupMetadata()
        {
            return std::unique_ptr<RdKafka::ConsumerGroupMetadata>(m_Consumer->groupMetadata());
        }

        struct Position : boost::noncopyable
        {
            std::vector<RdKafka::TopicPartition*> vector;
            Position()
            {
            }
            Position(std::vector<RdKafka::TopicPartition*>&& aFrom)
            : vector(std::move(aFrom))
            {
            }
            Position(Position&& aFrom)
            : vector(std::move(aFrom.vector))
            {
            }
            Position& operator=(Position&& aFrom)
            {
                clear();
                vector = std::move(aFrom.vector);
                aFrom.vector.clear();
                return *this;
            }
            void clear()
            {
                if (!vector.empty()) {
                    RdKafka::TopicPartition::destroy(vector);
                    vector.clear();
                }
            }
            ~Position()
            {
                clear();
            }
        };

        // get consumer position information
        Position position()
        {
            std::vector<RdKafka::TopicPartition*> sPosition;
            check(m_Consumer->assignment(sPosition), "Consumer: assignment");
            check(m_Consumer->position(sPosition), "Consumer: position");
            return Position(std::move(sPosition));
        }

        // position from broker (commited or earliest available)
        // FIXME: respect auto.offset.reset if query_watermark_offsets used
        Position broker_position()
        {
            std::vector<RdKafka::TopicPartition*> sPosition;
            check(m_Consumer->assignment(sPosition), "Consumer: assignment");
            for (auto& x : sPosition) {
                std::vector<RdKafka::TopicPartition*> sTmp;
                sTmp.push_back(x);
                auto sCode = m_Consumer->committed(sTmp, TIMEOUT);
                check(sCode, "Consumer: position/commited");
                if (sCode == RdKafka::ERR_NO_ERROR and x->offset() == RdKafka::Topic::OFFSET_INVALID) {
                    int64_t sBegin = -1;
                    int64_t sEnd   = -1;
                    check(m_Consumer->query_watermark_offsets(x->topic(), x->partition(), &sBegin, &sEnd, TIMEOUT), "Consumer: position/watermarks");
                    x->set_offset(sBegin);
                }
            }
            return Position(std::move(sPosition));
        }

        void seek(const Position& aPos)
        {
            for (auto& x : aPos.vector) {
                check(m_Consumer->seek(*x, TIMEOUT), "Consumer: seek");
            }
        }

        void rewind()
        {
            seek(broker_position());
        }

        void pause()
        {
            auto sPosition = position();
            check(m_Consumer->pause(sPosition.vector), "Consumer: pause");
        }

        void resume()
        {
            auto sPosition = position();
            check(m_Consumer->resume(sPosition.vector), "Consumer: resume");
        }

        void sync()
        {
            const auto sCode = m_Consumer->commitSync();
            if (sCode != RdKafka::ERR__NO_OFFSET)
                check(sCode, "Consumer: commitSync");
        }

        bool is_failed() const
        {
            std::string sTmp;
            return m_Consumer->fatal_error(sTmp) != RdKafka::ERR_NO_ERROR;
        }
    };

    class Producer
    {
        const std::string                  m_Topic;
        std::unique_ptr<RdKafka::Conf>     m_Config;
        std::unique_ptr<RdKafka::Producer> m_Producer;

    public:
        Producer(const Options&     aOptions,
                 const std::string& aTopic,
                 RdKafka::EventCb*  aEvent = nullptr)
        : m_Topic(aTopic)
        , m_Config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL))
        {
            std::string sErr;
            configure(m_Config.get(), aOptions);

            if (aEvent) {
                if (m_Config->set("event_cb", aEvent, sErr) != RdKafka::Conf::CONF_OK) {
                    throw std::invalid_argument("Kafka::Producer: can't set event_cb: " + sErr);
                }
            }

            m_Producer.reset(RdKafka::Producer::create(m_Config.get(), sErr));
            if (!m_Producer)
                throw std::invalid_argument("Kafka::Producer: can't create producer: " + sErr);

            ensure_topic(m_Producer.get(), aTopic, "Producer");

            if (aOptions.count("transactional.id") > 0)
                check(m_Producer->init_transactions(TIMEOUT), "Producer: init_transactions");
        }

        using Headers = std::vector<std::pair<std::string, std::string>>;

        // aPartition can be RdKafka::Topic::PARTITION_UA
        void push(int32_t aPartition, const std::string_view aKey, const std::string_view aValue, const Headers& aHeaders = {})
        {
            std::unique_ptr<RdKafka::Headers> sHeaders;
            if (!aHeaders.empty()) {
                sHeaders.reset(RdKafka::Headers::create());
                for (auto& [sKey, sValue] : aHeaders)
                    check(sHeaders->add(sKey, sValue), "Producer: prepare header");
            }

            auto sCode = m_Producer->produce(m_Topic,
                                             aPartition,
                                             RdKafka::Producer::RK_MSG_COPY,
                                             (void*)aValue.data(), aValue.size(),
                                             (void*)aKey.data(), aKey.size(),
                                             0 /* timestamp */,
                                             sHeaders.get() /* headers */,
                                             0 /* opaque */);
            if (sCode == RdKafka::ERR_NO_ERROR)
                sHeaders.release(); // headers will be freed/deleted if the produce() call succeeds
            check(sCode, "Producer: produce");
            poll();
        }

        // call to handle callbacks
        void poll(int aTimeout = 0)
        {
            m_Producer->poll(aTimeout);
        }

        void flush()
        {
            check(m_Producer->flush(TIMEOUT), "Producer: flush");
        }

        void begin()
        {
            check(m_Producer->begin_transaction(), "Producer: begin_transaction");
        }

        void send_offsets(Consumer& aConsumer)
        {
            auto sGroupMetadata = aConsumer.groupMetadata();
            auto sOffsets       = aConsumer.position();
            check(m_Producer->send_offsets_to_transaction(
                      sOffsets.vector,
                      sGroupMetadata.get(), -1),
                  "Producer: send_offsets_to_transaction");
        }

        // user must rewind consumer on exception
        void commit()
        {
            while (true) {
                std::unique_ptr<RdKafka::Error> sError(m_Producer->commit_transaction(-1));
                if (!sError)
                    return;
                if (sError->is_retriable()) {
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);
                    continue;
                }
                if (sError->txn_requires_abort()) {
                    std::string                     sAbortStr = "; successfully aborted";
                    std::unique_ptr<RdKafka::Error> sAbort(m_Producer->abort_transaction(-1));
                    if (sAbort)
                        sAbortStr = "; abort_transaction failed as well: " + to_string(sAbort.get());
                    throw std::runtime_error("Kafka::Producer: fail to commit: " + to_string(sError.get()) + sAbortStr);
                }
                // sError->is_fatal())
                throw std::runtime_error("Kafka::Producer: fatal error: " + to_string(sError.get()));
            }
        }

        void rollback()
        {
            check(m_Producer->abort_transaction(-1), "Producer: abort_transaction");
        }

        bool is_failed() const
        {
            std::string sTmp;
            return m_Producer->fatal_error(sTmp) != RdKafka::ERR_NO_ERROR;
        }
    };
} // namespace Kafka
