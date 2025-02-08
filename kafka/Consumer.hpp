#pragma once

#include "Kafka.hpp"

namespace Kafka
{
    class Consumer : public boost::noncopyable
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
            std::unique_ptr<RdKafka::Message> sMessage(m_Consumer->consume(aTimeout));
            if (sMessage) {
                if (sMessage->err() == RdKafka::ERR_NO_ERROR) {
                    DEBUG("got message, "
                          << "topic: " << sMessage->topic_name()
                          << ", partition: " << sMessage->partition()
                          << ", offset: " << sMessage->offset()
                          << ", key:" << Help::key(sMessage));
                } else {
                    DEBUG("got error: " << sMessage->errstr());
                }
            }
            return sMessage;
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
            INFO("commit: all");
            const auto sCode = m_Consumer->commitSync();
            if (sCode != RdKafka::ERR__NO_OFFSET)
                check(sCode, "Consumer: commitSync");
        }

        void sync(const std::string& aTopic, int32_t aPartition, int64_t aOffset)
        {
            INFO("commit: topic: " << aTopic << ", partition: " << aPartition << ", offset: " << aOffset);
            Position sOffsets;
            sOffsets.vector.push_back(RdKafka::TopicPartition::create(aTopic, aPartition, aOffset));

            const auto sCode = m_Consumer->commitSync(sOffsets.vector);
            if (sCode != RdKafka::ERR__NO_OFFSET)
                check(sCode, "Consumer: commitSync");
        }

        void sync(QueuePtr aQueue, void* aOpaque, const std::string& aTopic, int32_t aPartition, int64_t aOffset)
        {
            INFO("commit: topic: " << aTopic << ", partition: " << aPartition << ", offset: " << aOffset);
            using Offsets = std::unique_ptr<rd_kafka_topic_partition_list_t, void (*)(rd_kafka_topic_partition_list_t*)>;
            Offsets sOffsets(rd_kafka_topic_partition_list_new(1), rd_kafka_topic_partition_list_destroy);
            auto    sPartition = rd_kafka_topic_partition_list_add(sOffsets.get(), aTopic.c_str(), aPartition);
            sPartition->offset = aOffset;

            const auto sCode = rd_kafka_commit_queue(
                m_Consumer->c_ptr(),
                sOffsets.get(),
                aQueue->c_ptr(),
                nullptr /* callback*/,
                aOpaque);
            check(sCode, "Consumer: commit queue");
        }

        bool is_failed() const
        {
            std::string sTmp;
            return m_Consumer->fatal_error(sTmp) != RdKafka::ERR_NO_ERROR;
        }

        QueuePtr queue()
        {
            return Queue::CreateConsumer(m_Consumer->c_ptr());
        }
    };

}