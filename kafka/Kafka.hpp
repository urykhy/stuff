#pragma once

#include <librdkafka/rdkafkacpp.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

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
                std::string sErr = "Kafka::" + aMsg + ": " + to_string(aErr);
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
                 RdKafka::RebalanceCb* aRebalance = nullptr)
        : m_Config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL))
        {
            std::string sErr;
            configure(m_Config.get(), aOptions);

            if (aRebalance) {
                if (m_Config->set("rebalance_cb", aRebalance, sErr) != RdKafka::Conf::CONF_OK) {
                    throw std::invalid_argument("Kafka::Consumer: can't set rebalance_cb: " + sErr);
                }
            }

            m_Consumer.reset(RdKafka::KafkaConsumer::create(m_Config.get(), sErr));
            if (!m_Consumer)
                throw std::invalid_argument("Kafka::Consumer: can't create consumer: " + sErr);

            ensure_topic(m_Consumer.get(), aTopic, "Consumer");

            m_Topics.push_back(aTopic);
            check(m_Consumer->subscribe(m_Topics), "Consumer: subscribe");
        }

        auto consume(int aTimeout = TIMEOUT)
        {
            return std::unique_ptr<RdKafka::Message>(m_Consumer->consume(aTimeout));
        }

        auto groupMetadata()
        {
            return std::unique_ptr<RdKafka::ConsumerGroupMetadata>(m_Consumer->groupMetadata());
        }

        struct Position
        {
            std::vector<RdKafka::TopicPartition*> vector;
            ~Position()
            {
                RdKafka::TopicPartition::destroy(vector);
            }
        };

        Position position()
        {
            std::vector<RdKafka::TopicPartition*> sPosition;
            check(m_Consumer->assignment(sPosition), "Consumer: assignment");
            check(m_Consumer->position(sPosition), "Consumer: position");
            return {sPosition};
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

        void rewind()
        {
            check(m_Consumer->subscribe(m_Topics), "Consumer: rewind topic");
        }

        void sync()
        {
            check(m_Consumer->commitSync(), "Consumer: commitSync");
        }
    };

    class Producer
    {
        const std::string                  m_Topic;
        std::unique_ptr<RdKafka::Conf>     m_Config;
        std::unique_ptr<RdKafka::Producer> m_Producer;

    public:
        Producer(const Options& aOptions, const std::string& aTopic)
        : m_Topic(aTopic)
        , m_Config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL))
        {
            configure(m_Config.get(), aOptions);

            std::string sErr;
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
        }

        void flush()
        {
            check(m_Producer->flush(TIMEOUT), "Producer: flush");
        }

        void begin()
        {
            check(m_Producer->begin_transaction(), "Producer: begin_transaction");
        }

        void commit(Consumer& aConsumer)
        {
            auto sGroupMetadata = aConsumer.groupMetadata();
            auto sOffsets       = aConsumer.position();
            check(m_Producer->send_offsets_to_transaction(
                      sOffsets.vector,
                      sGroupMetadata.get(), -1),
                  "Producer: send_offsets_to_transaction");

            while (true) {
                std::unique_ptr<RdKafka::Error> sError(m_Producer->commit_transaction(-1));
                if (!sError)
                    return;
                if (sError->is_retriable()) {
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);
                    continue;
                }
                aConsumer.rewind();
                if (sError->txn_requires_abort()) {
                    std::string                     sAbortStr = "; successfully aborted";
                    std::unique_ptr<RdKafka::Error> sAbort(m_Producer->abort_transaction(-1));
                    if (sAbort)
                        sAbortStr = "; abort_transaction failed as well: " + to_string(sAbort.get());
                    throw std::runtime_error("Kafka::Producer: fail to commit: " + to_string(sError.get()) + sAbortStr);
                }
                if (sError->is_fatal()) {
                    throw std::runtime_error("Kafka::Producer: fatal error: " + to_string(sError.get()));
                }
            }
        }

        void rollback()
        {
            check(m_Producer->abort_transaction(-1), "Producer: abort_transaction");
        }
    };
} // namespace Kafka
