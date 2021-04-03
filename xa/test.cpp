#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Store.hpp"

using namespace XA;

BOOST_AUTO_TEST_SUITE(xa)
BOOST_AUTO_TEST_CASE(store)
{
    Store sTest;
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1",
                       .value     = "value1"}),
        Store::Result::SUCCESS);

    // reuse txn id
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1a",
                       .value     = "value1"}),
        Store::Result::CONFLICT);

    BOOST_CHECK_EQUAL(sTest.status("file1:user1"), Txn::Status::PREPARE);
    BOOST_CHECK(sTest.get("user1") == std::nullopt);

    // try run 2nd transaction to update same user
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user1:a",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1",
                       .value     = "value1"}),
        Store::Result::CONFLICT);

    // prepared transaction in list
    const std::vector<std::string> sExpected{{"file1:user1"}};
    std::vector<std::string>       sPrepared = sTest.list(); // list prepared
    BOOST_CHECK_EQUAL_COLLECTIONS(sPrepared.begin(), sPrepared.end(), sExpected.begin(), sExpected.end());

    BOOST_CHECK_EQUAL(sTest.next_serial(), 1);
    BOOST_CHECK_EQUAL(sTest.commit("file1:user1"), Store::Result::SUCCESS);
    BOOST_CHECK_EQUAL(sTest.status("file1:user1"), Txn::Status::COMMITED);
    BOOST_CHECK_EQUAL(sTest.next_serial(), 2);

    std::vector<std::string> sCommited = sTest.list(Txn::Status::COMMITED); // list commited transactions
    BOOST_CHECK_EQUAL_COLLECTIONS(sCommited.begin(), sCommited.end(), sExpected.begin(), sExpected.end());

    BOOST_CHECK_EQUAL(sTest.get("user1").value().data, "value1");
    BOOST_CHECK_EQUAL(sTest.get("user1").value().version, 1);

    // backup, clear, restore
    std::string sDump = sTest.backup();
    sTest.clear();
    sTest.restore(sDump);
    BOOST_CHECK_EQUAL(sTest.get("user1").value().data, "value1");
    BOOST_CHECK_EQUAL(sTest.get("user1").value().version, 1);

    // trim
    sTest.trim(0);
    BOOST_CHECK_EQUAL(sTest.status("file1:user1"), Txn::Status::UNKNOWN);
}
BOOST_AUTO_TEST_SUITE(manager)
BOOST_AUTO_TEST_CASE(simple)
{
}
BOOST_AUTO_TEST_SUITE_END() // manager
BOOST_AUTO_TEST_SUITE_END()
