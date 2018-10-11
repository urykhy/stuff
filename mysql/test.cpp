#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <Client.hpp>
#include <Pool.hpp>
#include <Updateable.hpp>
#include <Upload.hpp>
#include <Quote.hpp>
#include <Format.hpp>

#include <TimeMeter.hpp>
#include <iostream>
#include <cassert>

// g++ test.cpp -I. `mariadb_config --include` `mariadb_config --libs` -lcctz -I/usr/include/cctz -I../threads/ -lboost_filesystem -lboost_system -lboost_unit_test_framework -pthread -ggdb -std=c++14 -O3
// ./a.out -l all -t MySQL/prepare

struct Entry
{
    int emp_no = 0;
    int salary = 0;
    std::string from_date;
    std::string to_date;
};

MySQL::Config cfg{"sql1.mysql", 3306, "root", "", "employees"};

BOOST_AUTO_TEST_SUITE(MySQL)
BOOST_AUTO_TEST_CASE(simple)
{
    MySQL::Connection c(cfg);
    std::cout << "connected" << std::endl;
    std::list<Entry> sData;
    Util::TimeMeter m1;
    c.Query("select emp_no, salary, from_date, to_date from salaries");
    c.Use([&sData](const MySQL::Row& aRow){
        sData.push_back(Entry{aRow.as_int(0), aRow.as_int(1), aRow.as_str(2), aRow.as_str(3)});
        //std::cout << aRow.as_int(0) << " = " << aRow.as_int(1) << ", from " << aRow.as_str(2) << " to " << aRow.as_str(3) << std::endl;
    });
    std::cout << "elapsed " << m1.duration().count()/1000/1000.0 << " ms" << std::endl;
    std::cout << "got " << sData.size() << " rows" << std::endl;
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
    assert(nullptr == xc);
    std::cout << "pool size " << sPool.size() << std::endl;
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
    s.Use([i = 0](const auto* aBind, size_t aCount) mutable {
        const std::string sNumber((const char*)aBind[0].buffer, *aBind[0].length);
        const std::string sName((const char*)aBind[1].buffer, *aBind[1].length);
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
}
BOOST_AUTO_TEST_CASE(quote)
{
    BOOST_CHECK_EQUAL(MySQL::Quote("123"), "123");
    BOOST_CHECK_EQUAL(MySQL::Quote("'123'"), "\\'123\\'");
    BOOST_CHECK_EQUAL(MySQL::Quote("\"123\""), "\\\"123\\\"");
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
BOOST_AUTO_TEST_SUITE_END()
