#pragma once

#include <librdkafka/rdkafka.h>
#include <librdkafka/rdkafkacpp.h>

#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/noncopyable.hpp>

#include <parser/Atoi.hpp>
#include <unsorted/Log4cxx.hpp>

namespace Kafka {

    inline log4cxx::LoggerPtr sLogger = Logger::Get("kafka");

    using Options         = std::map<std::string, std::string>;
    constexpr int TIMEOUT = 1000; // 1s in ms

    namespace Help {
        using Message = std::unique_ptr<RdKafka::Message>;

        std::string_view key(const Message& aMsg)
        {
            return std::string_view(static_cast<const char*>(aMsg->key_pointer()), aMsg->key_len());
        }
        std::string_view key(const rd_kafka_message_t* aMsg)
        {
            return std::string_view(static_cast<const char*>(aMsg->key), aMsg->key_len);
        }
        std::string_view value(const Message& aMsg)
        {
            return std::string_view(static_cast<const char*>(aMsg->payload()), aMsg->len());
        }
        std::string_view value(const rd_kafka_message_t* aMsg)
        {
            return std::string_view(static_cast<const char*>(aMsg->payload), aMsg->len);
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

        inline void check(rd_kafka_resp_err_t aCode, const std::string& aMsg)
        {
            if (aCode != RD_KAFKA_RESP_ERR_NO_ERROR) {
                throw std::runtime_error("Kafka::" + aMsg + ": " + rd_kafka_err2str(aCode));
            }
        }

        inline void configure(RdKafka::Conf* aKafka, const Options& aOptions)
        {
            std::string sErr;
            for (auto& [key, value] : aOptions) {
                INFO("configure " << key << " = " << value);
                if (key == ":conf_set_events") {
                    rd_kafka_conf_set_events(aKafka->c_ptr_global(), Parser::Atoi<int>(value));
                    continue;
                }
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
            INFO("topic " << aTopic << " exists");
        }
    }; // namespace

    // get events from kafka using pipe
    class Queue : public boost::noncopyable
    {
        rd_kafka_queue_t* m_Queue;

    public:
        static std::shared_ptr<Queue> CreateProducer(rd_kafka_s* aParent)
        {
            return std::make_shared<Queue>(rd_kafka_queue_get_main(aParent));
        }
        static std::shared_ptr<Queue> CreateConsumer(rd_kafka_s* aParent)
        {
            return std::make_shared<Queue>(rd_kafka_queue_get_consumer(aParent));
        }

        Queue(rd_kafka_queue_t* aQueue)
        : m_Queue(aQueue)
        {
            if (!m_Queue) {
                throw std::runtime_error("Kafka::Queue: fail to create");
            }
        }
        ~Queue()
        {
            rd_kafka_queue_destroy(m_Queue);
        }
        void io_event_enable(int fd)
        {
            rd_kafka_queue_io_event_enable(m_Queue, fd, "1", 1);
        }
        rd_kafka_event_t* consume()
        {
            return rd_kafka_queue_poll(m_Queue, 0);
        }
        rd_kafka_queue_t* c_ptr()
        {
            return m_Queue;
        }
    };

    using QueuePtr = std::shared_ptr<Queue>;

} // namespace Kafka
