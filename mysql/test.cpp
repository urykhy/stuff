
#include <Client.hpp>
#include <Pool.hpp>
#include <Updateable.hpp>
#include <Upload.hpp>

#include <TimeMeter.hpp>
#include <iostream>
#include <cassert>

// g++ test.cpp -I. `mariadb_config --include` `mariadb_config --libs` -lcctz -I/usr/include/cctz -I../threads/ -lboost_filesystem -lboost_system -pthread -ggdb -std=c++14 -O3

struct Entry
{
    int emp_no = 0;
    int salary = 0;
    std::string from_date;
    std::string to_date;
};

int main(void)
{
    MySQL::Config cfg{"sql1.mysql", 3306, "root", "", "employees"};
#if 0
    // test1
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

    // test2
    {
        Util::Pool<MySQL::Connection> sPool([cfg](){
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

    // test3
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
#endif
    // test4
    /*
        create table newdata (
            `ID` int(11) NOT NULL auto_increment,
            `Name` char(35) NOT NULL default '',
            PRIMARY KEY  (`ID`)
        ) ENGINE=NDBCLUSTER DEFAULT CHARSET=utf8;
     */
    {
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

    return 0;
}
