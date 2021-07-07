#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <cassert>
#include <iostream>

#include "Client.hpp"
#include "Format.hpp"
#include "MessageQueue.hpp"
#include "Mock.hpp"
#include "Quote.hpp"
#include "TaskQueue.hpp"
#include "Updateable.hpp"
#include "Upload.hpp"

#include <container/Pool.hpp>
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

MySQL::Config cfg{"mysql-master", 3306, "root", "root", "employees", 10, "mysql-test-simple"};

BOOST_AUTO_TEST_SUITE(MySQL)
BOOST_AUTO_TEST_CASE(simple)
{
    MySQL::Connection c(cfg);
    BOOST_CHECK_EQUAL(c.ping(), true); // connected
    std::list<Entry> sData;
    Time::XMeter     m1;
    c.Query("SELECT emp_no, salary, from_date, to_date FROM salaries");
    c.Use([&sData](const MySQL::Row& aRow) {
        sData.push_back(Entry{aRow[0].as_int64(), aRow[1].as_int64(), aRow[2].as_string(), aRow[3].as_string()});
    });
    BOOST_TEST_MESSAGE("elapsed " << m1.duration().count() / 1000 / 1000.0 << " ms");
    BOOST_TEST_MESSAGE("got " << sData.size() << " rows");

    // ensure program name set
    bool sAttr = false;
    c.Query("SELECT * from performance_schema.session_connect_attrs WHERE attr_name='program_name' AND attr_value='mysql-test-simple'");
    c.Use([&sAttr](const MySQL::Row& aRow) {
        sAttr = 1;
    });
    BOOST_CHECK_EQUAL(sAttr, true);

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

        static void        parse(Container& aDest, const MySQL::Row& aRow) { aDest[aRow[0].as_string()] = aRow[1].as_string(); }
        static std::string query(Timestamp aTimestamp) { return "select dept_no, dept_name from departments"; }
        static Timestamp   merge(Container& aSrc, Container& aDst) { std::swap(aSrc, aDst); return 0; }
    };

    MySQL::Connection              c(cfg);
    MySQL::Updateable<Departments> upd;
    upd.update(c);
    auto r = upd.find("d008");
    BOOST_CHECK(r);
    BOOST_CHECK_EQUAL(r.value(), "Research");

    // dump and restore
    cbor::omemstream sDump;
    upd.dump(&sDump);
    BOOST_TEST_MESSAGE("serialized to " << sDump.str().size() << " bytes");

    cbor::imemstream               sRestore(sDump.str());
    MySQL::Updateable<Departments> upd2;
    upd2.restore(&sRestore);
    r = upd2.find("d006");
    BOOST_CHECK_EQUAL(r.value(), "Quality Management");
}
BOOST_AUTO_TEST_CASE(upload)
{
    MySQL::Connection c(cfg);
    c.Query("USE test");
    c.Query("TRUNCATE TABLE test_data");

    MySQL::Upload::Consumer sWorker("/tmp", cfg);
    MySQL::Upload::Producer sQueue("/tmp");
    Threads::Group          tg; // create last one. automagical `wait` in Threads::Group d-tor
    sWorker.start(tg);

    MySQL::Upload::List sList;
    sList.push_back("USE test");
    sList.push_back("INSERT INTO test_data VALUES (0, 'test1'),(0, 'test2'),(0,'test3')");
    sList.push_back("INSERT INTO test_data VALUES (0, 'testX1'),(0, 'testX2'),(0,'testX3')");

    const Time::Zone  sZone(cctz::utc_time_zone());
    const std::string sName = sZone.format(::time(NULL), Time::ISO);
    sQueue.push(sName, sList);
    Threads::sleep(2);

    unsigned sRowCount = 0;
    c.Query("select count(1) from test_data");
    c.Use([&sRowCount](const MySQL::Row& sRow) mutable {
        sRowCount = sRow[0].as_int64();
    });
    BOOST_CHECK_EQUAL(sRowCount, 6);
}
BOOST_AUTO_TEST_CASE(prepare)
{
    MySQL::Connection c(cfg);
    //MySQL::Statment s=c.Prepare("select * from departments limit 6");
    //s.Execute();
    MySQL::Statment s = c.Prepare("select * from departments where dept_no > ? and dept_no < ? order by dept_no");
    s.Execute("d004", "d007");
    s.Use([i = 0](const auto& aRow) mutable {
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

    MySQL::Statment s1 = c.Prepare("select * from salaries where from_date=? and emp_no > ? order by emp_no");
    s1.Execute("1997-11-28", 494301);
    s1.Use([i = 0](const auto& aRow) mutable {
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
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(TaskQueue)
BOOST_AUTO_TEST_CASE(simple)
{
    // insert tasks:  INSERT INTO task_queue(task) VALUES ('one'),('two'),('three');
    // refresh tasks: UPDATE task_queue SET status='new' WHERE task IN ('one','two','three');
    size_t sCount = 0;
    using namespace std::chrono_literals;
    struct TestHandler : MySQL::TaskQueue::HandlerFace
    {
        size_t& m_Count;
        TestHandler(size_t& aCount)
        : m_Count(aCount)
        {}

        bool process(const MySQL::TaskQueue::Task& task, MySQL::TaskQueue::updateCookieCB&& api) override
        {
            BOOST_TEST_MESSAGE("process task " << task.task << " with cookie " << task.cookie);
            m_Count++;
            return true;
        }
        void report(const char* e) noexcept override
        {
            BOOST_TEST_MESSAGE("exception: " << e);
        }
        virtual ~TestHandler() {}
    };
    TestHandler sHandler(sCount);

    MySQL::TaskQueue::Config sQueueCfg;
    MySQL::Config            sCfg = MySQL::Config{"mysql-master", 3306, "root", "root", "test"};
    MySQL::Connection        sConnection(sCfg);

    MySQL::TaskQueue::Manager sQueue(sQueueCfg, &sConnection, &sHandler);
    Threads::Group            sGroup;
    sQueue.start(sGroup);

    std::this_thread::sleep_for(1s);
    BOOST_CHECK_EQUAL(sCount, 3);
}
BOOST_AUTO_TEST_CASE(mock)
{
    struct TestHandler : MySQL::TaskQueue::HandlerFace
    {
        size_t& m_Count;
        TestHandler(size_t& aCount)
        : m_Count(aCount)
        {}

        bool process(const MySQL::TaskQueue::Task& task, MySQL::TaskQueue::updateCookieCB&& api) override
        {
            BOOST_CHECK_EQUAL(task.id, 12);
            BOOST_CHECK_EQUAL(task.task, "mock task");
            BOOST_CHECK_EQUAL(task.cookie, "existing cookie");
            api("updated cookie");
            m_Count++;
            return true;
        }
        void report(const char* e) noexcept override
        {
            BOOST_TEST_MESSAGE("exception: " << e);
        }
        virtual ~TestHandler() {}
    };
    size_t      sCount = 0;
    TestHandler sHandler(sCount);

    MySQL::Mock::SqlSet sExpectedSQL{
        {"BEGIN", {}},
        {"SELECT id, task, worker, cookie FROM task_queue WHERE status = 'new' OR (status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)) ORDER BY id ASC LIMIT 1 FOR UPDATE",
         {{"12", "mock task", "", "existing cookie"}}},
        {"UPDATE task_queue SET status = 'started', worker = 'test' WHERE id = 12", {}},
        {"COMMIT", {}},
        {"UPDATE task_queue SET cookie = 'updated cookie' WHERE id = 12", {}},
        {"UPDATE task_queue SET status = 'done' WHERE id = 12", {}},
        {"BEGIN", {}},
        {"SELECT id, task, worker, cookie FROM task_queue WHERE status = 'new' OR (status = 'started' AND updated < DATE_SUB(NOW(), INTERVAL 1 HOUR)) ORDER BY id ASC LIMIT 1 FOR UPDATE", {}},
        {"ROLLBACK", {}}};
    MySQL::Mock sMock(sExpectedSQL);

    MySQL::TaskQueue::Config  sQueueCfg;
    MySQL::TaskQueue::Manager sQueue(sQueueCfg, sMock, &sHandler);
    Threads::Group            sGroup;
    sQueue.start(sGroup);

    std::this_thread::sleep_for(100ms);
    BOOST_CHECK_EQUAL(sCount, 1);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(MessageQueue)
BOOST_AUTO_TEST_CASE(simple)
{
    MySQL::MessageQueue::Producer::Config sProducerCfg;

    MySQL::Config     sCfg = MySQL::Config{"mysql-master", 3306, "root", "root", "test"};
    MySQL::Connection sConnection(sCfg);

    sConnection.Query("TRUNCATE TABLE message_state");
    sConnection.Query("TRUNCATE TABLE message_queue");

    MySQL::MessageQueue::Producer sProducer(sProducerCfg, &sConnection);
    BOOST_CHECK_EQUAL(sProducer.insert("task 1", "a"), MySQL::MessageQueue::Producer::OK);
    BOOST_CHECK_EQUAL(sProducer.insert("task 1", "a"), MySQL::MessageQueue::Producer::OK);
    BOOST_CHECK_EQUAL(sProducer.insert("task 2", "b"), MySQL::MessageQueue::Producer::OK);

    MySQL::MessageQueue::Consumer::Config     sConsumerCfg;
    MySQL::MessageQueue::Consumer             sConsumer(sConsumerCfg, &sConnection);
    auto                                      sTasks    = sConsumer.select();
    const MySQL::MessageQueue::Consumer::List sExpected = {{1, "task 1", "a"}, {2, "task 2", "b"}};
    BOOST_CHECK_EQUAL(sTasks == sExpected, true);
    sConsumer.update();
    sTasks = sConsumer.select();
    BOOST_CHECK(sTasks.empty());
}
BOOST_AUTO_TEST_SUITE_END()