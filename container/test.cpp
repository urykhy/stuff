#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <map>
#include <thread>

#include "Algorithm.hpp"
#include "IoC.hpp"
#include "ListArray.hpp"
#include "RequestQueue.hpp"
#include "Session.hpp"
#include "Collect.hpp"

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(Container)
BOOST_AUTO_TEST_CASE(Merge)
{
    std::map<int, double> mapA = {{0, 1.0}, {1, 2.0}};
    std::map<int, double> mapB = {{1, 1.5}, {2, 2.5}};
    std::map<int, double> mapR = {{0, 1.0}, {1, 3.5}, {2, 2.5}};

    merge(mapA, mapB, [](double lhs, double rhs) { return lhs + rhs; });
    BOOST_CHECK(mapA == mapR);
}
BOOST_AUTO_TEST_CASE(listArray)
{
    Container::ListArray<size_t> a(10);
    BOOST_CHECK(a.size() == 0);
    const size_t max = 25;
    for (size_t i = 0; i < max; i++)
        a.push_back(i);
    BOOST_CHECK(a.begin() == a.begin());
    BOOST_CHECK(a.end() == a.end());
    BOOST_CHECK(a.begin() != a.end());
    BOOST_CHECK(a.size() == max);
    size_t c = 0;
    for (const auto& x : a) {
        BOOST_CHECK(c++ == x);
    }
}
BOOST_AUTO_TEST_CASE(ioc)
{
    Container::IoC r;
    int            i = 10;
    r.Put(i);
    double j = 20;
    r.Put(j);

    BOOST_CHECK(10 == r.Get<int>());
    BOOST_CHECK(abs(20 - r.Get<double>()) < 0.001);
}
BOOST_AUTO_TEST_SUITE(request_queue)
BOOST_AUTO_TEST_CASE(timeout_on_get)
{
    container::RequestQueue<std::string> sQueue([](std::string& aRequest) {
        BOOST_CHECK_EQUAL(aRequest, "first");
    });
    sQueue.insert("first", 10);
    std::this_thread::sleep_for(20ms);
    BOOST_CHECK_EQUAL((bool)sQueue.get(), false);
}
BOOST_AUTO_TEST_CASE(timeout_on_timer)
{
    container::RequestQueue<std::string> sQueue([](std::string& aRequest) {
        BOOST_CHECK_EQUAL(aRequest, "first");
    });
    sQueue.insert("first", 10); // timeout is 10 ms
    std::this_thread::sleep_for(5ms);
    BOOST_CHECK_CLOSE((float)sQueue.eta(100), 5.0, 30);
    std::this_thread::sleep_for(15ms);
    sQueue.on_timer();
    BOOST_CHECK_EQUAL(sQueue.empty(), true);
    BOOST_CHECK_EQUAL(sQueue.size(), 0);
    BOOST_CHECK_EQUAL((bool)sQueue.get(), false);
    BOOST_CHECK_EQUAL(sQueue.eta(100), 100);
}
BOOST_AUTO_TEST_CASE(no_timeouts)
{
    container::RequestQueue<std::string> sQueue([](std::string& aRequest) {
        BOOST_CHECK(false);
    });
    sQueue.insert("first", 100);
    sQueue.insert("second", 100);
    sQueue.insert("third", 100);
    BOOST_CHECK_EQUAL(sQueue.empty(), false);
    BOOST_CHECK_EQUAL(sQueue.size(), 3);
    BOOST_CHECK_EQUAL(*sQueue.get(), "first");
    BOOST_CHECK_EQUAL(*sQueue.get(), "second");
    BOOST_CHECK_EQUAL(*sQueue.get(), "third");
    BOOST_CHECK_EQUAL(sQueue.empty(), true);
    BOOST_CHECK_EQUAL(sQueue.size(), 0);
}
BOOST_AUTO_TEST_SUITE_END() // request_queue
BOOST_AUTO_TEST_CASE(session)
{
    namespace S = Container::Session;
    S::Set __a1("a", std::string_view("one"));

    []() {
        BOOST_CHECK_EQUAL(S::get<std::string_view>("a").has_value(), true);
		BOOST_CHECK_EQUAL(S::get<std::string_view>("a").value(), "one");
		[](){
			S::Set __a2("b", std::string_view("two"));
			BOOST_CHECK_EQUAL(S::get<std::string_view>("b").has_value(), true);
		}();
		BOOST_CHECK_EQUAL(S::get<std::string_view>("b").has_value(), false);
    }();
}
BOOST_AUTO_TEST_CASE(is_uniq)
{
    BOOST_CHECK_EQUAL(Container::is_unique(std::vector({1,1})), false);
    BOOST_CHECK_EQUAL(Container::is_unique(std::vector({1,2,3,4,1})), false);

    BOOST_CHECK_EQUAL(Container::is_unique(std::vector({1})), true);
    BOOST_CHECK_EQUAL(Container::is_unique(std::vector({1,2})), true);
    BOOST_CHECK_EQUAL(Container::is_unique(std::vector({1,2,3,4})), true);
}
BOOST_AUTO_TEST_CASE(collect)
{
    using Data          = std::string;
    unsigned   sCounter = 0;

    const std::vector<Data> sTestData({"abc", "def", "ghi", "jkl", "mno", "pqr", "stu", "vwx", "yz", "123"});

    Container::Collect<Data> c(3, [&sCounter, &sTestData](std::vector<Data>& aData) {
        std::vector<Data> sExpected;
        switch (sCounter) {
        case 0: sExpected.assign(sTestData.begin(), sTestData.begin() + 3); break;
        case 1: sExpected.assign(sTestData.begin() + 3, sTestData.begin() + 6); break;
        case 2: sExpected.assign(sTestData.begin() + 6, sTestData.begin() + 9); break;
        case 3: sExpected.assign(sTestData.begin() + 9, sTestData.end()); break;
        }
        BOOST_CHECK_EQUAL_COLLECTIONS(aData.begin(), aData.end(), sExpected.begin(), sExpected.end());
        sCounter++;
    });

    for (auto& x : sTestData)
        c.insert(Data(x));

    BOOST_CHECK_EQUAL(sCounter, 3);
    BOOST_CHECK_EQUAL(c.size(), 1);
    BOOST_CHECK_EQUAL(c.idle(), false);

    c.flush();

    BOOST_CHECK_EQUAL(sCounter, 4);
    BOOST_CHECK_EQUAL(c.size(), 0);
    BOOST_CHECK_EQUAL(c.idle(), true);
}
BOOST_AUTO_TEST_SUITE_END()