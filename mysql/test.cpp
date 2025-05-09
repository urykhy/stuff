#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <cassert>
#include <iostream>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/use_future.hpp>

#include "Cacheable.hpp"
#include "Client.hpp"
#include "Coro.hpp"
#include "Format.hpp"
#include "MessageQueue.hpp"
#include "Mock.hpp"
#include "Quote.hpp"
#include "TaskQueue.hpp"
#include "Updateable.hpp"
#include "Upload.hpp"

#include <container/Pool.hpp>
#include <format/List.hpp>
#include <time/Meter.hpp>
#include <time/Time.hpp>

using namespace std::chrono_literals;

struct Entry
{
    int64_t     emp_no = 0;
    int64_t     salary = 0;
    std::string from_date;
    std::string to_date;
};

const bool gMySQLCleanup = []() { std::atexit(mysql_library_end); return true; }();

MySQL::Config cfg{.database = "employees", .program_name = "mysql-test-simple"};

BOOST_AUTO_TEST_SUITE(MySQL)
BOOST_AUTO_TEST_CASE(simple)
{
    MySQL::Connection c(cfg);
    BOOST_CHECK_EQUAL(c.ping(), true); // connected
    std::list<Entry> sData;
    Time::Meter      m1;
    c.Query("SELECT emp_no, salary, from_date, to_date FROM salaries LIMIT 100");
    c.Use([&sData](const MySQL::Row& aRow) {
        sData.push_back(Entry{aRow[0], aRow[1], aRow[2], aRow[3]});
    });
    BOOST_CHECK_EQUAL(100, sData.size());
    BOOST_TEST_MESSAGE("elapsed " << m1.get().to_double() << " s");

    // ensure program name set
    bool sAttr = false;
    c.Query("SELECT * from performance_schema.session_connect_attrs WHERE attr_name='program_name' AND attr_value='mysql-test-simple'");
    c.Use([&sAttr](const MySQL::Row& aRow) {
        sAttr = 1;
    });
    BOOST_CHECK_EQUAL(sAttr, true);

    try {
        c.Query("select 123 from");
    } catch (const MySQL::Error& e) {
        BOOST_TEST_MESSAGE("exception: " << e.what());
        BOOST_CHECK_EQUAL(e.decode(), MySQL::Error::BAD_QUERY);
    }

    c.close();
    BOOST_CHECK_EQUAL(c.ping(), false); // not connected
}
BOOST_AUTO_TEST_CASE(pool)
{
    using Ptr = std::shared_ptr<MySQL::Connection>;
    Container::ProducePool<Ptr> sPool([]() { return std::make_shared<MySQL::Connection>(cfg); }, [](Ptr c) -> bool { return c->ping(); });
    auto                        xc = sPool.get();
    xc->Query("select version()");
    xc->Use([](const MySQL::Row& aRow) {
        BOOST_CHECK_EQUAL("version()", aRow[0].name());
        BOOST_TEST_MESSAGE(aRow[0].as_string());
    });
    sPool.insert(xc);
    BOOST_CHECK_EQUAL(xc, nullptr);
    BOOST_CHECK_EQUAL(sPool.size(), 1);
}
BOOST_AUTO_TEST_CASE(updateable)
{
    struct Departments
    {
        using Container = std::map<std::string, std::string>;
        using Timestamp = time_t;
        using Pair      = std::pair<std::string, std::string>;

        static Pair parse(const MySQL::Row& aRow)
        {
            return Pair(aRow[0].as_string(), aRow[1].as_string());
        }
        static std::string query(Timestamp aTimestamp)
        {
            return "SELECT dept_no, dept_name FROM departments WHERE dept_no > 'd004'";
        }
        static Timestamp merge(Container& aSrc, Container& aDst)
        {
            std::swap(aSrc, aDst);
            return 0;
        }
        static std::string fallback(const std::string& aKey)
        {
            return "SELECT dept_no, dept_name FROM departments WHERE dept_no = '" + aKey + "'";
        }
    };

    MySQL::Connection              c(cfg);
    MySQL::Updateable<Departments> upd;
    upd.update(c);
    BOOST_CHECK_EQUAL(upd.find("d008").value(), "Research");

    // check fallback
    BOOST_CHECK(!upd.find("d001"));
    BOOST_CHECK_EQUAL(upd.find("d001", c).value(), "Marketing");
    BOOST_CHECK_EQUAL(upd.find("d001").value(), "Marketing");
    BOOST_CHECK(!upd.find("nx-entry", c));

    // dump and restore
    cbor::omemstream sDump;
    upd.dump(&sDump);
    BOOST_TEST_MESSAGE("serialized to " << sDump.str().size() << " bytes");

    cbor::imemstream               sRestore(sDump.str());
    MySQL::Updateable<Departments> upd2;
    upd2.restore(&sRestore);
    BOOST_CHECK_EQUAL(upd2.find("d006").value(), "Quality Management");
}
BOOST_AUTO_TEST_CASE(once)
{
    MySQL::Connection c(MySQL::Config{.database = "test"});
    c.Query("TRUNCATE TABLE transaction_log");
    c.Query("TRUNCATE TABLE test_data");

    c.Query("INSERT INTO transaction_log VALUES ('test','foo','bar',DATE_SUB(NOW(), INTERVAL 3 DAY))");
    MySQL::Once::truncate(&c);
    unsigned sCount = 0;
    c.Query("SELECT COUNT(1) FROM transaction_log");
    c.Use([&sCount](const MySQL::Row& aRow) mutable { sCount = aRow[0]; });
    BOOST_CHECK_EQUAL(0, sCount);

    auto sInsert = [](auto aClient) { aClient->Query("INSERT INTO test_data (name) VALUES ('dummy')"); };
    MySQL::Once::transaction(&c, "test", "foo", "bar", sInsert);
    MySQL::Once::transaction(&c, "test", "foo", "bar", sInsert); // 2nd time

    sCount = 0;
    c.Query("SELECT COUNT(1) FROM test_data");
    c.Use([&sCount](const MySQL::Row& aRow) mutable { sCount = aRow[0]; });
    BOOST_CHECK_EQUAL(1, sCount);
}
BOOST_AUTO_TEST_CASE(upload)
{
    MySQL::Connection c(MySQL::Config{.database = "test"});
    c.Query("TRUNCATE TABLE transaction_log");
    c.Query("TRUNCATE TABLE test_data");

    MySQL::Upload::Consumer sWorker("/tmp", MySQL::Config{.database = "test"});
    MySQL::Upload::Producer sQueue("/tmp");
    Threads::Group          tg; // create last one. automagical `wait` in Threads::Group d-tor
    sWorker.start(tg);

    MySQL::Upload::List sList;
    sList.push_back("INSERT INTO test_data VALUES (0, 'test1'),(0, 'test2'),(0,'test3')");
    sList.push_back("INSERT INTO test_data VALUES (0, 'testX1'),(0, 'testX2'),(0,'testX3')");

    const Time::Zone  sZone(cctz::utc_time_zone());
    const std::string sName = sZone.format(::time(NULL), Time::ISO);
    sQueue.push(sName, sList);
    Threads::sleep(2);

    unsigned sRowCount = 0;
    c.Query("select count(1) from test_data");
    c.Use([&sRowCount](const MySQL::Row& aRow) mutable {
        sRowCount = aRow[0];
    });
    BOOST_CHECK_EQUAL(sRowCount, 6);

    c.Query("select created from transaction_log where service='uploader' and task='" + sName + ".upload.sql'");
    c.Use([](const MySQL::Row& aRow) mutable {
        BOOST_TEST_MESSAGE("transaction_log entry created at: " << aRow[0].as_string());
    });
}
BOOST_AUTO_TEST_CASE(prepare)
{
    MySQL::Connection c(cfg);
    // MySQL::Statement s=c.Prepare("select * from departments limit 6");
    // s.Execute();
    MySQL::Statement s = c.Prepare("select * from departments where dept_no > ? and dept_no < ? order by dept_no");
    s.Execute("d004", "d007");
    s.Use([i = 0](const auto& aRow, const auto& aMeta) mutable {
        BOOST_CHECK_EQUAL(aMeta[0], "dept_no");
        BOOST_CHECK_EQUAL(aMeta[1], "dept_name");
        const std::string_view sNumber = aRow[0];
        const std::string_view sName   = aRow[1];
        BOOST_TEST_MESSAGE(sNumber << " = " << sName);
        if (i == 0) {
            BOOST_CHECK_EQUAL(sNumber, "d005");
            BOOST_CHECK_EQUAL(sName, "Development");
        }
        if (i == 1) {
            BOOST_CHECK_EQUAL(sNumber, "d006");
            BOOST_CHECK_EQUAL(sName, "Quality Management");
        }
        i++;
        BOOST_CHECK(i <= 2);
    });

    MySQL::Statement s1 = c.Prepare("select * from salaries where from_date=? and emp_no > ? order by emp_no");
    s1.Execute("1997-11-28", 494301);
    s1.Use([i = 0](const auto& aRow, const auto& aMeta) mutable {
        BOOST_TEST_MESSAGE(aRow[0] << ' ' << aRow[1] << ' ' << aRow[2] << ' ' << aRow[3]);
        if (i == 0) {
            BOOST_CHECK_EQUAL(aRow[0], "495165");
            BOOST_CHECK_EQUAL(aRow[1], "48649");
        }
        i++;
    });
}
BOOST_AUTO_TEST_CASE(quote)
{
    BOOST_CHECK_EQUAL(MySQL::Quote("123"), "123");
    BOOST_CHECK_EQUAL(MySQL::Quote("'123'"), "\\'123\\'");
    BOOST_CHECK_EQUAL(MySQL::Quote("\"123\""), "\\\"123\\\"");
    BOOST_CHECK_EQUAL(MySQL::Quote("'Строка'"), "\\'Строка\\'");
}
struct FormatPolicy
{
    static size_t      max_size() { return 3; }
    static std::string table() { return "newdata"; }
    static std::string fields() { return "id, name"; }
    static std::string finalize() { return "ON DUPLICATE KEY UPDATE name=name"; }
    static void        format(std::ostream& aStream, const std::pair<int, std::string>& aData)
    {
        aStream << aData.first << ", '" << aData.second << "'";
    }
};
BOOST_AUTO_TEST_CASE(format)
{
    std::list<std::pair<int, std::string>> sData{{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}};
    std::list<std::string>                 sExpected{
                        {"INSERT INTO newdata (id, name) VALUES (1, 'one'), (2, 'two'), (3, 'three') ON DUPLICATE KEY UPDATE name=name"},
                        {"INSERT INTO newdata (id, name) VALUES (4, 'four') ON DUPLICATE KEY UPDATE name=name"}};
    const auto sResult = MySQL::Format<FormatPolicy>(sData);
    BOOST_CHECK_EQUAL_COLLECTIONS(sResult.begin(), sResult.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(mock)
{
    MySQL::Mock::SqlSet sExpectedSQL{
        {"SELECT id, task FROM task_queue1"},
        {"SELECT id, task FROM task_queue2",
         MySQL::Mock::Rows{.rows = {
                               {"12", "mock task"},
                               {"12", "mock task"},
                           },
                           .meta = {"id", "task"}}},
        {"COMMIT", MySQL::Error{"mock generated error", CR_SERVER_LOST}},
    };
    MySQL::Mock sMock(sExpectedSQL);

    MySQL::ConnectionFace* sClient = sMock;

    unsigned sCount = 0;
    sClient->Query("SELECT id, task FROM task_queue1");
    sClient->Use([&sCount](const MySQL::Row& aRow) {
        sCount++;
    });
    BOOST_CHECK_EQUAL(0, sCount);

    sClient->Query("SELECT id, task FROM task_queue2");
    sClient->Use([&sCount](const MySQL::Row& aRow) {
        BOOST_CHECK_EQUAL(12, aRow[0].as_uint64());
        BOOST_CHECK_EQUAL("mock task", aRow[1].as_string());
        BOOST_CHECK_EQUAL(aRow[0].name(), "id");
        BOOST_CHECK_EQUAL(aRow[1].name(), "task");
        sCount++;
    });
    BOOST_CHECK_EQUAL(2, sCount);

    try {
        sClient->Query("COMMIT");
        BOOST_CHECK(false);
    } catch (const MySQL::Error& e) {
        BOOST_CHECK_EQUAL(CR_SERVER_LOST, e.m_Errno);
        BOOST_CHECK_EQUAL(MySQL::Error::NETWORK, e.decode());
    }
}
BOOST_AUTO_TEST_CASE(cacheable)
{
    MySQL::Connection c(cfg);
    MySQL::Cacheable  sCache({}, &c);

    MySQL::CacheableQuery sQuery{
        .table = "salaries",
        .from  = "2002-07-01",
        .to    = "2002-07-10",
        .where = "emp_no BETWEEN 492590 AND 499961",
        .query = "SELECT from_date, sum(salary) FROM {0} WHERE from_date IN({1}) AND {2} GROUP BY 1",
        .parse = [](const MySQL::Row& aRow) { return std::pair(aRow[0].as_string(), aRow[1].as_string()); }};

    // run query and cache
    Time::Meter sMeter;
    auto        sResp = sCache(sQuery);
    BOOST_CHECK_EQUAL(9, sResp.size());
    BOOST_CHECK_EQUAL("1303398", sResp.find("2002-07-02")->second);
    BOOST_TEST_MESSAGE("elapsed " << sMeter.get().to_ms() << " ms");

    // ensure response cached
    auto [sMissing, sCached] = sCache.prepare(sQuery);
    BOOST_CHECK_EQUAL(0, sMissing.size());
    BOOST_CHECK_EQUAL(9, sCached.size());
    BOOST_CHECK_EQUAL("1303398", sCached.find("2002-07-02")->second);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(TaskQueue)
BOOST_AUTO_TEST_CASE(simple)
{
    using namespace std::chrono_literals;

    MySQL::TaskQueue::Config sQueueCfg{.reverse = true};
    MySQL::Config            sCfg = MySQL::Config{.database = "test"};
    MySQL::Connection        sConnection(sCfg);

    sConnection.Query("TRUNCATE TABLE task_queue");
    sConnection.Query("INSERT INTO task_queue(task) VALUES ('one'),('two'),('three')");

    MySQL::TaskQueue::Manager sQueue(sQueueCfg, &sConnection);
    BOOST_CHECK_EQUAL(3, sQueue.size());

    const std::vector<std::string> sExpected{"three", "two", "one"};
    std::vector<std::string>       sActual;
    while (true) {
        auto sTask = sQueue.get("test-worker");
        if (!sTask)
            break;
        sActual.push_back(sTask->task);
        BOOST_TEST_MESSAGE("process task " << sTask->task);
    }
    BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(), sExpected.end(), sActual.begin(), sActual.end());
}

BOOST_AUTO_TEST_CASE(mock)
{
    MySQL::Mock::SqlSet sExpectedSQL{
        {"BEGIN"},
        {"SELECT id, task, worker, cookie FROM task_queue WHERE status = 'new' OR (status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)) ORDER BY id ASC LIMIT 1 FOR UPDATE SKIP LOCKED",
         MySQL::Mock::Rows{.rows = {{"12", "mock task", "test", "existing cookie"}}, .meta = {"id", "task", "worker", "cookie"}}},
        {"UPDATE task_queue SET status = 'started', worker = 'test' WHERE id = 12"},
        {"COMMIT"},
        {"UPDATE task_queue SET cookie = 'updated cookie' WHERE id = 12 AND status IN ('started')"},
        {"UPDATE task_queue SET status = 'done' WHERE id = 12 AND status IN ('started','done')"},
        {"BEGIN"},
        {"SELECT id, task, worker, cookie FROM task_queue WHERE status = 'new' OR (status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)) ORDER BY id ASC LIMIT 1 FOR UPDATE SKIP LOCKED"},
        {"COMMIT"}};
    MySQL::Mock sMock(sExpectedSQL);

    MySQL::TaskQueue::Config  sQueueCfg;
    MySQL::TaskQueue::Manager sQueue(sQueueCfg, sMock);

    size_t sCount = 0;
    while (true) {
        auto sTask = sQueue.get("test");
        if (!sTask)
            break;

        BOOST_CHECK_EQUAL(sTask->id, 12);
        BOOST_CHECK_EQUAL(sTask->task, "mock task");
        BOOST_CHECK_EQUAL(sTask->cookie.value(), "existing cookie");
        sQueue.update(sTask->id, "updated cookie");
        sQueue.done(sTask->id, true);
        sCount++;
    }
    BOOST_CHECK_EQUAL(sCount, 1);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(MessageQueue)
BOOST_AUTO_TEST_CASE(simple)
{
    MySQL::MessageQueue::Producer::Config sProducerCfg;

    MySQL::Config     sCfg = MySQL::Config{.database = "test"};
    MySQL::Connection sConnection(sCfg);

    sConnection.Query("TRUNCATE TABLE mq_producer");
    sConnection.Query("TRUNCATE TABLE mq_consumer");
    sConnection.Query("TRUNCATE TABLE mq_data");

    MySQL::MessageQueue::Producer sProducer(sProducerCfg, &sConnection);
    sConnection.Query("BEGIN");
    BOOST_CHECK_EQUAL(sProducer.insert("task 1"), MySQL::MessageQueue::Producer::OK);
    BOOST_CHECK_EQUAL(sProducer.insert("task 1"), MySQL::MessageQueue::Producer::ALREADY);
    BOOST_CHECK_EQUAL(sProducer.insert("task 2"), MySQL::MessageQueue::Producer::OK);
    sConnection.Query("COMMIT");

    MySQL::MessageQueue::Consumer::Config     sConsumerCfg;
    MySQL::MessageQueue::Consumer             sConsumer(sConsumerCfg, &sConnection);
    auto                                      sTasks    = sConsumer.select();
    const MySQL::MessageQueue::Consumer::List sExpected = {{1, "task 1"}, {2, "task 2"}};
    BOOST_CHECK_EQUAL(sTasks == sExpected, true);
    sConsumer.update();
    sTasks = sConsumer.select();
    BOOST_CHECK(sTasks.empty());
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Coro)
BOOST_AUTO_TEST_CASE(simple)
{
    boost::asio::io_service sAsio;

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> boost::asio::awaitable<void> {
            std::list<Entry>        sData;
            MySQL::Coro::Connection sClient(cfg);
            co_await sClient.Open();
            Time::Meter sMeter;
            co_await sClient.Query("SELECT emp_no, salary, from_date, to_date FROM salaries LIMIT 100");
            co_await sClient.Use([&sData](const MySQL::Row& aRow) {
                sData.push_back(Entry{aRow[0], aRow[1], aRow[2], aRow[3]});
            });
            BOOST_CHECK_EQUAL(100, sData.size());
            BOOST_TEST_MESSAGE("elapsed " << sMeter.get().to_double() << " s");

            //
            try {
                co_await sClient.Query("select 123 from");
            } catch (const MySQL::Error& e) {
                BOOST_TEST_MESSAGE("exception: " << e.what());
                BOOST_CHECK_EQUAL(e.decode(), MySQL::Error::BAD_QUERY);
            }
        },
        boost::asio::use_future);

    sAsio.run_for(1000ms);
    BOOST_REQUIRE_EQUAL(sFuture.wait_for(0ms) == std::future_status::ready, true);
    sFuture.get();
}
BOOST_AUTO_TEST_SUITE_END()
