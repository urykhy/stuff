
#include <Client.hpp>
#include <TimeMeter.hpp>
#include <iostream>

// g++ test.cpp -I. `mariadb_config --include` `mariadb_config --libs` -pthread -ggdb -std=c++14 -O3

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

    return 0;
}
