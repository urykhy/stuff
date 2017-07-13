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

#include <iostream>
#include <Stat.hpp>
#include <tree.hpp>

#include <unistd.h>

int main(void)
{
    Stat::Main s;
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

    return 0;
}
