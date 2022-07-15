#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Transform.hpp"

#include <threads/Group.hpp>

/*
    kafkactl create topic test_source --partitions 3 --replication-factor 3
    kafkactl create topic test_destination --partitions 3 --replication-factor 3

    kafkactl describe consumer-group test_basic
    kafkactl describe consumer-group test_transform

    kafkactl consume test_source -b -e --print-headers --print-keys
*/

// options["debug"] = "all";
auto producerOptions(const std::string& aName, bool aTransactional)
{
    Kafka::Options sOpt;
    sOpt["acks"]              = "all";
    sOpt["bootstrap.servers"] = "broker-1.kafka";
    sOpt["client.id"]         = aName;
    if (aTransactional) {
        sOpt["transactional.id"]       = aName;
        sOpt["transaction.timeout.ms"] = "5000";
    }
    return sOpt;
}

auto consumerOptions(const std::string& aGroup, const std::string& aName)
{
    Kafka::Options sOpt;
    sOpt["auto.offset.reset"]  = "earliest";
    sOpt["bootstrap.servers"]  = "broker-1.kafka";
    sOpt["client.id"]          = aName;
    sOpt["enable.auto.commit"] = "false";
    sOpt["group.id"]           = aGroup;
    sOpt["group.instance.id"]  = aName;
    return sOpt;
}

void tryMessages(Kafka::Consumer& aConsumer, int32_t aCount)
{
    for (int i = 0; i < aCount; i++) {
        auto sMsg = aConsumer.consume();
        if (sMsg->err() == RdKafka::ERR_NO_ERROR) {
            BOOST_TEST_MESSAGE("got message"
                               << ": partition: " << sMsg->partition()
                               << "; offset: " << sMsg->offset()
                               << "; data: " << Kafka::Help::value(sMsg));
        }
    }
}

BOOST_AUTO_TEST_SUITE(kafka)
BOOST_AUTO_TEST_CASE(nx_topic)
{
    Kafka::Options sConfig;
    sConfig["bootstrap.servers"] = "broker-1.kafka";
    BOOST_CHECK_THROW(
        [sConfig]() {
            Kafka::Consumer sConsumer(sConfig, "nx-topic");
        }(),
        std::invalid_argument);
    BOOST_CHECK_THROW(
        [sConfig]() {
            Kafka::Producer sProducer(sConfig, "nx-topic");
        }(),
        std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(basic)
{
    Kafka::Consumer sConsumer(consumerOptions("test_basic", "test_basic/consumer"), "test_source");
    sConsumer.consume();

    const std::string              sKey   = "some-key";
    const std::string              sValue = "basic: " + std::to_string(::time(nullptr));
    const Kafka::Producer::Headers sHeaders{{"header1", "value1"}, {"header2", "value2"}};
    {
        Kafka::Producer sKafka(producerOptions("test_basic/producer", false), "test_source");
        sKafka.push(RdKafka::Topic::PARTITION_UA, sKey, sValue, sHeaders);
        sKafka.flush();
        BOOST_TEST_MESSAGE("data inserted");
    }

    Kafka::Help::Message sMsg;
    while (true) {
        sMsg = sConsumer.consume();
        if (sMsg->err() != RdKafka::ERR_NO_ERROR)
            continue;
        sConsumer.sync();
        BOOST_TEST_MESSAGE("got message: " << Kafka::Help::value(sMsg));
        if (Kafka::Help::value(sMsg) == sValue)
            break;
    };

    BOOST_CHECK_EQUAL(Kafka::Help::key(sMsg), sKey);
    BOOST_CHECK_EQUAL(Kafka::Help::value(sMsg), sValue);

    Kafka::Producer::Headers sActual;
    Kafka::Help::headers(sMsg, [&sActual](auto x, auto y) {
        sActual.emplace_back(x, std::string(y));
    });

    BOOST_REQUIRE_EQUAL(sHeaders.size(), sActual.size());
    for (unsigned i = 0; i < sHeaders.size(); i++) {
        BOOST_TEST_CHECKPOINT("Test header " << i);
        BOOST_CHECK_EQUAL(sHeaders[i].first, sActual[i].first);
        BOOST_CHECK_EQUAL(sHeaders[i].second, sActual[i].second);
    }
}
BOOST_AUTO_TEST_CASE(transform)
{
    Kafka::Transform::Config sConfig;
    sConfig.consumer.options = consumerOptions("test_transform", "test_transform/consumer");
    sConfig.producer.options = producerOptions("test_transform/producer", true);
    sConfig.max_size         = 5;

    // push some data to source queue
    {
        Kafka::Producer sKafka(producerOptions("test_transform/prepare", false), "test_source");
        for (unsigned i = 0; i < sConfig.max_size; i++) {
            sKafka.push(RdKafka::Topic::PARTITION_UA,
                        {},
                        "transform: " + std::to_string(::time(nullptr)) + ": " + std::to_string(i));
        }
        sKafka.flush();
        BOOST_TEST_MESSAGE("data inserted");
    }

    // transform
    auto sHandler = [](RdKafka::Message* aMsg, Kafka::Producer& aProducer) {
        static unsigned sCounter = 0;
        sCounter++;
        if (sCounter == 4)
            throw std::runtime_error("rollback requested from handler");
        if (aMsg == nullptr)
            return;
        std::string sPayload(static_cast<const char*>(aMsg->payload()), aMsg->len());
        BOOST_TEST_MESSAGE("got message"
                           << ": partition: " << aMsg->partition()
                           << "; offset: " << aMsg->offset()
                           << "; data: " << sPayload);
        std::string sResult("processed: " + sPayload);
        aProducer.push(RdKafka::Topic::PARTITION_UA, {}, sResult);
    };

    Kafka::Transform sKafka(sConfig);
    BOOST_CHECK_THROW(sKafka(sHandler), std::runtime_error); // ensure we interrupt transaction by handler request
    sKafka(sHandler);
}
BOOST_AUTO_TEST_CASE(rebalance)
{
    constexpr int TOPIC_COUNT = 3;
    constexpr int MSG_COUNT   = 6;

    std::atomic_bool sStage1 = false;

    Threads::Group sGroup;
    sGroup.start([&]() {
        BOOST_TEST_MESSAGE("1: begin transformation");

        Kafka::Transform::Config sConfig;
        sConfig.consumer.options = consumerOptions("test_rebalance", "test_rebalance/consumer-1");
        sConfig.producer.options = producerOptions("test_rebalance/producer", true);
        sConfig.max_size         = 100500; // unlim messages..
        sConfig.time_limit       = 10;

        bool             sSuccess = false;
        int              sCount   = 0;
        Kafka::Transform sTransform(sConfig);
        try {
            sTransform([&](RdKafka::Message* aMsg, Kafka::Producer& aProducer) {
                if (aMsg == nullptr)
                    return;
                std::string sPayload(static_cast<const char*>(aMsg->payload()), aMsg->len());
                BOOST_TEST_MESSAGE("1: transform message"
                                   << ": partition: " << aMsg->partition()
                                   << "; offset: " << aMsg->offset()
                                   << "; data: " << sPayload);
                std::string sResult("transformed: " + sPayload);
                aProducer.push(RdKafka::Topic::PARTITION_UA, {}, sResult);
                sCount++;
                if (sCount == MSG_COUNT) {
                    BOOST_TEST_MESSAGE("1: allow 2nd consumer");
                    sStage1 = true;
                }
            });
        } catch (const std::exception& e) {
            BOOST_CHECK_EQUAL("Kafka::Transform: rebalance", e.what());
            sSuccess = true;
        }
        BOOST_TEST(sSuccess, "rollback as expected");
    });
    sGroup.start([&]() {
        BOOST_TEST_MESSAGE("2: insert data");
        Kafka::Producer sKafka(producerOptions("test_rebalance/fill", false), "test_source");
        for (int i = 0; i < MSG_COUNT; i++) {
            sKafka.push(i % TOPIC_COUNT,
                        {},
                        "rebalance: " + std::to_string(::time(nullptr)) + ": " + std::to_string(i));
            sKafka.flush();
        }

        BOOST_TEST_MESSAGE("2: wait for transaction to start ...");
        using namespace std::chrono_literals;
        while (!sStage1)
            std::this_thread::sleep_for(100ms);

        BOOST_TEST_MESSAGE("2: start second consumer");
        Kafka::Consumer sConsumer(consumerOptions("test_rebalance", "test_rebalance/consumer-2"), "test_source");
        tryMessages(sConsumer, 10);

        BOOST_TEST_MESSAGE("2: finish second consumer");
    });
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()