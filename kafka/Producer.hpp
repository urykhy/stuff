#pragma once

#include "Consumer.hpp"

namespace Kafka {

    class Producer : public boost::noncopyable
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
        void push(int32_t aPartition, const std::string_view aKey, const std::string_view aValue,
                  const Headers& aHeaders = {}, void* aOpaque = nullptr, bool aPoll = true)
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
                                             aOpaque);
            if (sCode == RdKafka::ERR_NO_ERROR)
                sHeaders.release(); // headers will be freed/deleted if the produce() call succeeds
            check(sCode, "Producer: produce");
            if (aPoll) {
                poll();
            }
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

        QueuePtr queue()
        {
            return Queue::CreateProducer(m_Producer->c_ptr());
        }
    };

} // namespace Kafka