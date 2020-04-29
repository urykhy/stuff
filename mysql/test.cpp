#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Client.hpp"
#include "Pool.hpp"
#include "Updateable.hpp"
#include "Upload.hpp"
#include "Quote.hpp"
#include "Format.hpp"
#include "TaskQueue.hpp"

#include <time/Meter.hpp>
#include <iostream>
#include <cassert>

struct Entry
{
    int64_t emp_no = 0;
    int64_t salary = 0;
    std::string from_date;
    std::string to_date;
};

MySQL::Config cfg{"sql1.mysql", 3306, "root", "", "employees"};

BOOST_AUTO_TEST_SUITE(MySQL)
BOOST_AUTO_TEST_CASE(simple)
{
    MySQL::Connection c(cfg);
    BOOST_CHECK_EQUAL(c.ping(), true);  // connected
    std::list<Entry> sData;
    Time::XMeter m1;
    c.Query("select emp_no, salary, from_date, to_date from salaries");
    c.Use([&sData](const MySQL::Row& aRow){
        sData.push_back(Entry{aRow.as_int(0), aRow.as_int(1), aRow.as_str(2), aRow.as_str(3)});
        //std::cout << aRow.as_int(0) << " = " << aRow.as_int(1) << ", from " << aRow.as_str(2) << " to " << aRow.as_str(3) << std::endl;
    });
    std::cout << "elapsed " << m1.duration().count()/1000/1000.0 << " ms" << std::endl;
    std::cout << "got " << sData.size() << " rows" << std::endl;

    c.close();
    BOOST_CHECK_EQUAL(c.ping(), false);  // not connected
}
BOOST_AUTO_TEST_CASE(pool)
{
    Util::Pool<MySQL::Connection> sPool([](){
        return std::make_shared<MySQL::Connection>(cfg);
    }, [](auto c) -> bool {
        return c->ping();
    });
    auto xc = sPool.get();
    xc->Query("select version()");
    xc->Use([](const MySQL::Row& aRow){
        std::cout << aRow.as_str(0) << std::endl;
    });
    sPool.release(xc);
    BOOST_CHECK_EQUAL(xc, nullptr);
    BOOST_CHECK_EQUAL(sPool.size(), 1);
}
BOOST_AUTO_TEST_CASE(updateable)
{
    struct Departments
    {
        using Container = std::map<std::string, std::string>;
        static void parse(Container& aDest, const MySQL::Row& aRow) { aDest[aRow.as_str(0)] = aRow.as_str(1); }
        static std::string query() { return "select dept_no, dept_name from departments"; }
    };

    MySQL::Connection c(cfg);
    MySQL::Updateable<Departments> upd;
    upd.update(c);
    auto r = upd.find("d008");
    if (r)
        std::cout << "got " << r.value() << std::endl;
};
BOOST_AUTO_TEST_CASE(upload)
{
/*
    CREATE TABLE newdata (
        `ID` INT NOT NULL AUTO_INCREMENT,
        `Name` CHAR(35) NOT NULL DEFAULT '',
        PRIMARY KEY  (`ID`)
    ) ENGINE=NDBCLUSTER DEFAULT CHARSET=utf8;
*/
    MySQL::Upload::Worker sWorker("/tmp/", cfg);
    MySQL::Upload::Queue  sQueue("/tmp/");
    Threads::Group tg;    // create last one
    sWorker.start(tg);

    MySQL::Upload::Queue::List sList;
    sList.push_back("USE test");
    sList.push_back("insert into newdata values (0, 'test1'),(0, 'test2'),(0,'test3')");
    sList.push_back("insert into newdata values (0, 'testX1'),(0, 'testX2'),(0,'testX3')");

    sQueue.push(sList);
    Threads::sleep(2);
    // automagical `wait` in Threads::Group d-tor
}
BOOST_AUTO_TEST_CASE(prepare)
{
    MySQL::Connection c(cfg);
    //MySQL::Statment s=c.Prepare("select * from departments limit 6");
    //s.Execute();
    MySQL::Statment s=c.Prepare("select * from departments where dept_no > ? and dept_no < ? order by dept_no");
    s.Execute("d004","d007");
    s.Use([i = 0](const auto& aRow) mutable {
        const std::string sNumber = aRow[0];
        const std::string sName   = aRow[1];
        std::cout << sNumber << " = " << sName << std::endl;
        if (i == 0)
        {
            BOOST_CHECK_EQUAL(sNumber, "d005");
            BOOST_CHECK_EQUAL(sName,   "Development");
        }
        if (i == 1)
        {
            BOOST_CHECK_EQUAL(sNumber, "d006");
            BOOST_CHECK_EQUAL(sName,   "Quality Management");
        }
        i++;
        BOOST_CHECK(i <= 2);
    });

    MySQL::Statment s1=c.Prepare("select * from salaries where from_date=? and emp_no > ? order by emp_no");
    s1.Execute("1997-11-28", 494301);
    s1.Use([i=0](const auto& aRow) mutable {
        std::cout << aRow[0] << ' ' << aRow[1] << ' ' << aRow[2] << ' ' << aRow[3] << std::endl;
        if (i == 0)
        {
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
    static std::string table()    { return "newdata"; }
    static std::string fields()   { return "id, name"; }
    static std::string finalize() { return "ON DUPLICATE KEY UPDATE name=name"; }
    static void        format(std::ostream& aStream, const std::pair<int, std::string>& aData) {
        aStream << aData.first << ", '" << aData.second << "'";
    }
};
BOOST_AUTO_TEST_CASE(format)
{
    std::list<std::pair<int, std::string>> sData{{1,"one"},{2, "two"},{3, "three"},{4, "four"}};
    std::list<std::string> sExpected{{"INSERT INTO newdata (id, name) VALUES (1, 'one'), (2, 'two'), (3, 'three') ON DUPLICATE KEY UPDATE name=name"},
                                     {"INSERT INTO newdata (id, name) VALUES (4, 'four') ON DUPLICATE KEY UPDATE name=name"}};
    const auto sResult = MySQL::Format<FormatPolicy>(sData);
    BOOST_CHECK_EQUAL_COLLECTIONS(sResult.begin(), sResult.end(), sExpected.begin(), sExpected.end());
}
BOOST_AUTO_TEST_CASE(task_queue)
{
    // insert tasks:  INSERT INTO task_queue(task) VALUES ('one'),('two'),('three');
    // refresh tasks: UPDATE task_queue SET status='new';
    size_t sCount = 0;
    using namespace std::chrono_literals;
    struct TestHandler : MySQL::TaskQueue::HandlerFace
    {
        size_t&     m_Count;
        TestHandler(size_t& aCount) : m_Count(aCount) {}

        std::string prepare(const std::string& task) noexcept override {
            BOOST_TEST_MESSAGE("prepare task " << task);
            return task;
        }
        bool process(const std::string& task, const std::string& hint) override {
            BOOST_TEST_MESSAGE("process task " << task << " with hint " << hint);
            m_Count++;
            return true;
        }
        void report(const char* e) noexcept override {
            BOOST_TEST_MESSAGE("exception: " << e);
        }
        virtual    ~TestHandler() {}
    };
    TestHandler sHandler(sCount);

    MySQL::TaskQueue::Config sQueueCfg;
    sQueueCfg.mysql = MySQL::Config{"sql1.mysql", 3306, "root", "", "test"};

    MySQL::TaskQueue::Manager sQueue(sQueueCfg, &sHandler);
    Threads::Group sGroup;
    sQueue.start(sGroup);

    std::this_thread::sleep_for(1s);
    BOOST_CHECK_EQUAL(sCount, 3);
}
BOOST_AUTO_TEST_SUITE_END()
