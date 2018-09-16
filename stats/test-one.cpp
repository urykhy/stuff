/*

   ./parser.py > tree.hpp
   g++ -std=c++14 test-one.cpp -I.

   proof-of-concept
   generate c++ stats from yml description

   support types:
    count
    time(with seconds ago, requests per second)
    bool
   + format to graphite


 */

#define STAT_ENABLE_TAGS

#include <iostream>
#include <Stat.hpp>
#include <tree.hpp>

#include <unistd.h>

Stat::Main s;
namespace Tag
{
    STAT_DECLARE_TAG(s.common.start, stat_start);
    STAT_DECLARE_TAG(s.visits.http.error, stat_bool);
}

int main(void)
{
    s.common.start.set(time(0)-4);
    s.common.format(std::cout, "common");

    s.visits.match.hits.update(1);
    s.visits.match.refresh.set(time(0)-10);
    s.visits.match.refresh_time.set(4.456);
    s.visits.match.size.set(123);
    s.visits.match.format(std::cout, "X1");

    s.actions.http.file_count.set(10);
    s.actions.http.loading.update(2);
    s.actions.http.loading.update(3);
    s.actions.http.loading.update(4);
    s.actions.http.processing.update(0.1);
    s.actions.http.processing.update(0.2);
    s.actions.http.processing.update(0.3);
    s.actions.http.error.set();
    s.actions.http.file_count.set(7);
    s.actions.http.format(std::cout, "X2");

    s.actions.http.processing.update(0.6);
    s.actions.http.error.clear();
    s.actions.http.file_count.set(6);
    s.actions.http.format(std::cout, "X3");

    {
        const auto v = time(0)-10;
        Stat::set<Tag::stat_start>(v);
        if (s.common.start.time != v) { std::cout << "incorrect set for Time: " << s.common.start.time << " != " << v; }

        s.visits.http.error.set();
        Stat::clear<Tag::stat_bool>();
        if (s.visits.http.error.flag != false) { std::cout << "incorrect clear for Bool: " << s.visits.http.error.flag << " != " << false; }
    }

    return 0;
}
