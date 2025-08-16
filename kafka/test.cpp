#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <boost/asio.hpp>

#include "Coro.hpp"
#include "Factory.hpp"
#include "Registry.hpp"
#include "Transform.hpp"

#include <threads/Group.hpp>

using namespace std::chrono_literals;

/*
 * use Taskfile.yml for kafka ops
 *
 * task kafkactl:install
 * task topics:create
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
        sOpt["transaction.timeout.ms"] = "30000";
    }
    return sOpt;
}

auto consumerOptions(const std::string& aName, const std::string& aGroup)
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
    aConsumer.sync();
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
    const std::string              sKey   = "some-key";
    const std::string              sValue = "basic: " + std::to_string(::time(nullptr));
    const Kafka::Producer::Headers sHeaders{{"header1", "value1"}, {"header2", "value2"}};
    {
        Kafka::Producer sKafka(producerOptions("basic/producer", false), "t_source");
        sKafka.push(RdKafka::Topic::PARTITION_UA, sKey, sValue, sHeaders);
        sKafka.flush();
        BOOST_TEST_MESSAGE("data inserted");
    }

    Kafka::Consumer      sConsumer(consumerOptions("basic/consumer", "g_basic"), "t_source");
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
BOOST_AUTO_TEST_CASE(coro)
{
    const std::string              sKey   = "some-key-coro";
    const std::string              sValue = "basic: " + std::to_string(::time(nullptr));
    const Kafka::Producer::Headers sHeaders{{"header1", "value1"}, {"header2", "value2"}};

    BOOST_TEST_MESSAGE("test with message: " << sValue);

    {
        Kafka::Coro::Meta       sMeta{};
        auto                    sProducer = Kafka::Factory::MakeProducer("basic/producer", "test_source");
        boost::asio::io_service sAsio;
        boost::asio::co_spawn(
            sAsio,
            [sProducer, &sMeta, &sKey, &sValue, &sHeaders]() -> boost::asio::awaitable<void> {
                co_await sProducer->start();
                sMeta = co_await sProducer->push(RdKafka::Topic::PARTITION_UA, sKey, sValue, sHeaders);
                sProducer->stop();
            },
            boost::asio::detached);
        sAsio.run_for(1000ms);
        BOOST_CHECK_EQUAL(sMeta.status, RD_KAFKA_MSG_STATUS_PERSISTED);
        BOOST_CHECK_EQUAL(sMeta.error, RD_KAFKA_RESP_ERR_NO_ERROR);
        BOOST_TEST_MESSAGE("data inserted to partition " << sMeta.partition << ", at offset " << sMeta.offset);
    }
    {
        bool    sConsumed  = false;
        bool    sCommited  = false;
        int32_t sPartition = -1;
        int64_t sOffset    = -1;

        auto sConsumer =
            Kafka::Factory::MakeConsumer("basic/consumer", "g_basic",
                                         [&sConsumed, &sValue, &sPartition, &sOffset](const rd_kafka_message_t* aMsg) {
                                             if (aMsg->err != RD_KAFKA_RESP_ERR_NO_ERROR)
                                                 return;
                                             if (Kafka::Help::value(aMsg) == sValue) {
                                                 sConsumed  = true;
                                                 sPartition = aMsg->partition;
                                                 sOffset    = aMsg->offset;
                                             }
                                         });
        boost::asio::io_service sAsio;
        boost::asio::co_spawn(
            sAsio,
            [&, sConsumer]() -> boost::asio::awaitable<void> {
                co_await sConsumer->start();
                boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                while (!sConsumed) {
                    sTimer.expires_from_now(100ms);
                    co_await sTimer.async_wait(boost::asio::use_awaitable);
                }
                auto sCode = co_await sConsumer->sync("t_source", sPartition, sOffset + 1);
                BOOST_CHECK_EQUAL(sCode, RD_KAFKA_RESP_ERR_NO_ERROR);
                sCommited = true;
                sConsumer->stop();
                sAsio.stop();
            },
            boost::asio::detached);
        sAsio.run_for(6000ms);
        BOOST_CHECK(sConsumed);
        BOOST_CHECK(sCommited);
    }
}

struct SerdesTest
{
    std::string name;
    int         type;

    void avro_encode(avro::GenericRecord& aRecord) const
    {
        aRecord.field("name") = name;
        aRecord.field("type") = type;
    }
};

BOOST_AUTO_TEST_CASE(serdes)
{
    const std::string sDef  = R"(
    {
        "type": "record",
        "name": "cpx",
        "version": 1,
        "fields" : [
            {"name": "name", "type": "string"},
            {"name": "type", "type": "int"}
        ]
    }
    )";
    const std::string sName = "test_serdes_5";
    Kafka::Registry   sRegistry;

    auto sCurrent = sRegistry.GetOrCreate(sName, sDef);
    BOOST_TEST_MESSAGE("using schema: " << sCurrent->definition());

    const std::string sJson = R"({"name":"foo","type":123})";
    auto              sAvro = sRegistry.Encode(sJson, sCurrent.get());
    std::string_view  sAvroView(&sAvro[0], sAvro.size());
    std::string       sActual = sRegistry.Decode(sAvroView);
    BOOST_TEST_MESSAGE("actual message: " << sActual);
    BOOST_CHECK_EQUAL(sJson, sActual);

    // w/o json
    {
        SerdesTest       sTest{.name = "bar", .type = 321};
        auto             sAvro = sRegistry.Encode(sTest, sCurrent.get());
        std::string_view sAvroView(&sAvro[0], sAvro.size());
        std::string      sActual = sRegistry.Decode(sAvroView);
        BOOST_TEST_MESSAGE("decoded message: " << sActual);

        const std::string sJson = R"({"name":"bar","type":321})";
        BOOST_CHECK_EQUAL(sJson, sActual);
    }
}
BOOST_AUTO_TEST_CASE(transform)
{
    constexpr unsigned MSG_COUNT = 5;
    // push some data to source queue
    {
        Kafka::Producer sKafka(producerOptions("transform/prepare", false), "t_source");
        for (unsigned i = 0; i < MSG_COUNT; i++) {
            sKafka.push(RdKafka::Topic::PARTITION_UA,
                        {},
                        "transform: " + std::to_string(::time(nullptr)) + ": " + std::to_string(i));
        }
        sKafka.flush();
        BOOST_TEST_MESSAGE("data inserted");
    }

    // handler
    unsigned sPushCount = 0;
    auto     sHandler   = [&sPushCount](RdKafka::Message* aMsg, Kafka::Producer& aProducer) {
        static unsigned sCounter = 0;
        sCounter++;
        if (sCounter == 4)
            throw std::runtime_error("rollback requested from handler");
        if (aMsg == nullptr)
            return;
        std::string sPayload(static_cast<const char*>(aMsg->payload()), aMsg->len());
        BOOST_TEST_MESSAGE("transform message"
                                 << ": partition: " << aMsg->partition()
                                 << "; offset: " << aMsg->offset()
                                 << "; data: " << sPayload);
        std::string sResult("processed: " + sPayload);
        aProducer.push(RdKafka::Topic::PARTITION_UA, {}, sResult);
        sPushCount++;
    };

    // transform
    Kafka::Transform::Config sConfig;
    sConfig.consumer.options = consumerOptions("transform/consumer", "g_transform");
    sConfig.producer.options = producerOptions("transform/producer", true);
    sConfig.max_size         = MSG_COUNT;
    Kafka::Transform sKafka(sConfig);
    BOOST_CHECK_THROW(sKafka(sHandler), std::runtime_error); // ensure we interrupt transaction by handler request
    // resume (auto recover)
    sPushCount = 0;
    sKafka(sHandler);
    BOOST_CHECK_EQUAL(sPushCount, MSG_COUNT);
}
BOOST_AUTO_TEST_CASE(rebalance)
{
    constexpr int TOPIC_COUNT = 3;
    constexpr int MSG_COUNT   = 10;

    std::atomic_bool sStage1 = false;

    Threads::Group sGroup;
    sGroup.start([&]() {
        BOOST_TEST_MESSAGE("1: begin transformation");

        Kafka::Transform::Config sConfig;
        sConfig.consumer.options = consumerOptions("rebalance/consumer-1", "g_rebalance");
        sConfig.producer.options = producerOptions("rebalance/producer", true);
        // sConfig.producer.options["debug"]="broker,topic,msg,eos";
        sConfig.max_size   = 100500; // unlim messages..
        sConfig.time_limit = 20;     // less than transaction.timeout.ms

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
                if (sCount >= 2 and !sStage1) {
                    BOOST_TEST_MESSAGE("1: allow 2nd consumer");
                    sStage1 = true;
                }
                if (sStage1)
                    std::this_thread::sleep_for(1s);
            });
        } catch (const std::exception& e) {
            BOOST_CHECK_EQUAL("Kafka::Transform: rebalance", e.what());
            sSuccess = true;
            sTransform.recover();
            BOOST_TEST_MESSAGE("1: recovered");
        }
        BOOST_TEST(sSuccess, "rollback as expected");
    });
    sGroup.start([&]() {
        BOOST_TEST_MESSAGE("2: insert data");
        Kafka::Producer sKafka(producerOptions("rebalance/fill", false), "t_source");
        for (int i = 0; i < MSG_COUNT; i++) {
            sKafka.push(i % TOPIC_COUNT,
                        {},
                        "rebalance: " + std::to_string(::time(nullptr)) + ": " + std::to_string(i));
            sKafka.flush();
        }

        BOOST_TEST_MESSAGE("2: wait for transaction to start ...");
        while (!sStage1)
            std::this_thread::sleep_for(100ms);

        BOOST_TEST_MESSAGE("2: start second consumer");
        Kafka::Consumer sConsumer(consumerOptions("rebalance/consumer-2", "g_rebalance"), "t_source");
        tryMessages(sConsumer, 10);

        BOOST_TEST_MESSAGE("2: finish second consumer");
    });
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()
