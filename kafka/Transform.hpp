#pragma once

#include "Kafka.hpp"

#include <unsorted/Raii.hpp>

namespace Kafka {
    struct Transform : private RdKafka::RebalanceCb
    {
        struct Config
        {
            struct
            {
                Options     options;
                std::string topic = "t_source";
            } consumer;
            struct
            {
                Options     options;
                std::string topic = "t_destination";
            } producer;

            unsigned max_size   = 10;
            time_t   time_limit = 25; // less than transaction.timeout.ms
        };

    private:
        const Config m_Config;
        Consumer     m_Consumer;
        Producer     m_Producer;
        bool         m_Rollback{false};
        bool         m_Rewind{false};

        std::atomic<unsigned> m_RebalanceId{0};

    public:
        Transform(const Config& aConfig)
        : m_Config(aConfig)
        , m_Consumer(aConfig.consumer.options, aConfig.consumer.topic, this)
        , m_Producer(aConfig.producer.options, aConfig.producer.topic)
        {
        }

        using Handler = std::function<void(RdKafka::Message*, Producer&)>;

        void operator()(Handler&& aHandler)
        {
            recover();

            auto sRebalanceCheck = [this, sRebalance = m_RebalanceId.load()]() mutable {
                if (sRebalance != m_RebalanceId) {
                    throw std::runtime_error("Kafka::Transform: rebalance");
                }
            };

            m_Producer.begin();
            const time_t sStarted = time(nullptr);
            unsigned     sPos     = 0;
            m_Rollback            = true;
            m_Rewind              = true;

            while (sPos < m_Config.max_size and sStarted + m_Config.time_limit > time(nullptr)) {
                auto sMsg = m_Consumer.consume();
                sRebalanceCheck();
                if (sMsg->err() == RdKafka::ERR_NO_ERROR) {
                    aHandler(sMsg.get(), m_Producer);
                    sPos++;
                }
            }

            aHandler(nullptr, m_Producer);       // flush (if handler collect some state)
            m_Producer.send_offsets(m_Consumer); // send_offsets_to_transaction
            m_Rollback = false;
            m_Producer.commit();
            m_Rewind = false;
        }

        void recover()
        {
            if (m_Rollback) {
                m_Producer.rollback();
                m_Rollback = false;
            }
            if (m_Rewind) {
                m_Consumer.rewind();
                m_Rewind = false;
            }
        }

        bool is_failed() const
        {
            return m_Consumer.is_failed() || m_Producer.is_failed();
        }

    private:
        // TODO: support rebalance_protocol() == "COOPERATIVE"
        void rebalance_cb(RdKafka::KafkaConsumer*                aConsumer,
                          RdKafka::ErrorCode                     aErrCode,
                          std::vector<RdKafka::TopicPartition*>& aPartitions) override
        {
            if (aErrCode == RdKafka::ERR__ASSIGN_PARTITIONS) {
                aConsumer->assign(aPartitions);
            } else {
                m_RebalanceId++;
                aConsumer->unassign();
            }
        }
    };
} // namespace Kafka