#pragma once

#include <memory>

#include "Coro.hpp"

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

    using ProducerPtr = std::shared_ptr<Kafka::Coro::Producer>;
    inline ProducerPtr MakeProducer(const std::string& aID, const std::string& aGroup)
    {
        auto [sOpt, sTopic] = GetOptions("producer", aGroup);
        sOpt["client.id"]   = aID;
        return std::make_shared<Kafka::Coro::Producer>(std::move(sOpt), sTopic);
    }

    using ConsumerPtr = std::shared_ptr<Kafka::Coro::Consumer>;
    inline ConsumerPtr MakeConsumer(const std::string& aID, const std::string& aGroup, Kafka::Coro::Consumer::Callback&& aCallback)
    {
        auto [sOpt, sTopic]       = GetOptions("consumer", aGroup);
        sOpt["client.id"]         = aID;
        sOpt["group.id"]          = aGroup;
        sOpt["group.instance.id"] = aID;
        return std::make_shared<Kafka::Coro::Consumer>(std::move(sOpt), sTopic, std::move(aCallback));
    }

} // namespace Kafka::Factory
