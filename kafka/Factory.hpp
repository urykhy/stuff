#pragma once

#include <memory>

#include "Coro.hpp"
#include "Registry.hpp"

#include <parser/Json.hpp>

#define FILE_NO_ARCHIVE
#include <file/File.hpp>

namespace Kafka::Factory {

    inline std::pair<Kafka::Options, std::string> GetOptions(const std::string& aKind, const std::string& aName)
    {
        Kafka::Options sOpt;

        auto sStr  = File::to_string("../Factory.yaml");
        auto sJson = Parser::Json::parse(sStr);

        if (!sJson.isMember(aKind)) {
            throw std::invalid_argument("Kafka::Factory: kind " + aKind + " not found");
        }
        for (auto& sItem : sJson[aKind]) {
            if (sItem.isMember("name") and sItem["name"] == aName) {
                const auto& sOptions = sItem["options"];
                for (auto i = sOptions.begin(); i != sOptions.end(); i++) {
                    sOpt[i.key().asString()] = i->asString();
                }
                return std::pair(sOpt, sItem["topic"].asString());
            }
        }
        throw std::invalid_argument("Kafka::Factory: name " + aName + " not found for kind " + aKind);
    };

    template <class T>
    class Producer
    {
        Kafka::Registry&                m_Registry;
        std::unique_ptr<Serdes::Schema> m_Schema;
        std::shared_ptr<Coro::Producer> m_Producer;

    public:
        Producer(Options&& aOptions, const std::string& aTopic, std::unique_ptr<Serdes::Schema> aSchema, Kafka::Registry& aRegistry)
        : m_Registry(aRegistry)
        , m_Schema(std::move(aSchema))
        {
            m_Producer = std::make_shared<Coro::Producer>(std::move(aOptions), aTopic);
        }
        boost::asio::awaitable<void> start()
        {
            co_await m_Producer->start();
        }
        void stop()
        {
            m_Producer->stop();
        }

        boost::asio::awaitable<Coro::Meta>
        push(int32_t                aPartition,
             const std::string_view aKey, const T& aValue,
             const Kafka::Producer::Headers& aHeaders = {})
        {
            auto             sAvro = m_Registry.Encode(aValue, m_Schema.get());
            std::string_view sAvroView(&sAvro[0], sAvro.size());
            co_return co_await m_Producer->push(aPartition, aKey, sAvroView, aHeaders);
        }
    };

    template <class T>
    inline std::shared_ptr<Producer<T>> MakeProducer(const std::string& aID, const std::string& aGroup, Kafka::Registry& aRegistry)
    {
        auto [sOpt, sTopic] = GetOptions("producer", aGroup);
        sOpt["client.id"]   = aID;
        auto sIt            = sOpt.find("serdesSchema");
        if (sIt == sOpt.end()) {
            throw std::invalid_argument("Kafka::Factory: name " + aGroup + ", not found param: serdesSchema");
        }
        auto sSchema = aRegistry.GetSchema(sIt->second);
        sOpt.erase(sIt);
        return std::make_shared<Producer<T>>(std::move(sOpt), sTopic, std::move(sSchema), aRegistry);
    }

    template <class T>
    class Consumer
    {
        std::shared_ptr<Coro::Consumer> m_Consumer;

    public:
        struct Message
        {
            T                         parsed;
            const rd_kafka_message_t* source = nullptr;
        };
        using Callback = std::function<void(Message&&)>;

        Consumer(Options&& aOptions, const std::string& aTopic, Kafka::Registry& aRegistry, Callback&& aCallback)
        {
            m_Consumer = std::make_shared<Coro::Consumer>(std::move(aOptions), aTopic, [aRegistry = std::ref(aRegistry), aCallback = std::move(aCallback)](const rd_kafka_message_t* aMsg) {
                Message sMessage;
                sMessage.parsed = aRegistry.get().Decode<T>(Kafka::Help::value(aMsg));
                sMessage.source = aMsg;
                aCallback(std::move(sMessage));
            });
        }
        boost::asio::awaitable<void> start()
        {
            co_await m_Consumer->start();
        }
        void stop()
        {
            m_Consumer->stop();
        }
        boost::asio::awaitable<rd_kafka_resp_err_t> sync(const std::string& aTopic, int32_t aPartition, int64_t aOffset)
        {
            co_return co_await m_Consumer->sync(aTopic, aPartition, aOffset);
        }
    };

    template <class T>
    inline std::shared_ptr<Consumer<T>> MakeConsumer(const std::string& aID, const std::string& aGroup, Kafka::Registry& aRegistry, typename Consumer<T>::Callback&& aCallback)
    {
        auto [sOpt, sTopic]       = GetOptions("consumer", aGroup);
        sOpt["client.id"]         = aID;
        sOpt["group.id"]          = aGroup;
        sOpt["group.instance.id"] = aID;
        return std::make_shared<Consumer<T>>(std::move(sOpt), sTopic, aRegistry, std::move(aCallback));
    }

} // namespace Kafka::Factory
