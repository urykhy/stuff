/*

   ./parser.py > Test.hpp
   g++ -std=c++14 test-one.cpp -I.


 */

#include <iostream>
#include <Stat.hpp>
#include <tree.hpp>

#include <unistd.h>

int main(void)
{
    Stat::Main s;
    s.common.start.set(time(0)-4);

    s.actions.http.file_count.set(10);
    s.actions.http.loading.update(2);
    s.actions.http.loading.update(3);
    s.actions.http.loading.update(4);
    s.actions.http.processing.update(0.1);
    s.actions.http.processing.update(0.2);
    s.actions.http.processing.update(0.3);
    s.actions.http.error.set();
    s.actions.http.file_count.set(7);

    s.visits.match.hits.update(1);
    s.visits.match.refresh.set(time(0)-10);
    s.visits.match.size.set(123);

    s.format(std::cout, "test");

    return 0;
}
