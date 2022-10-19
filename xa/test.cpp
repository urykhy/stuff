#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Manager.hpp"
#include "Store.hpp"

using namespace XA;

BOOST_AUTO_TEST_SUITE(store)
BOOST_AUTO_TEST_CASE(basic)
{
    Store sTest;
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1",
                       .value     = "value1"}),
        Store::Result::SUCCESS);

    // reuse txn name for same operation
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1",
                       .value     = "value1"}),
        Store::Result::SUCCESS);

    // create same user in other transaction
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file2:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1",
                       .value     = "value1"}),
        Store::Result::CONFLICT);

    // reuse txn name to create other user
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user2",
                       .value     = "value1"}),
        Store::Result::CONFLICT);

    BOOST_CHECK_EQUAL(sTest.status("file1:user1"), Txn::Status::PREPARE);
    BOOST_CHECK(sTest.get("user1") == std::nullopt);

    // try run 2nd transaction to update same user
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file2:user1",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user1",
                       .value     = "value1"}),
        Store::Result::CONFLICT);

    // pending transactions list
    const std::vector<std::string> sExpected{{"file1:user1"}};
    std::vector<std::string>       sPrepared = sTest.list(); // list prepared
    BOOST_CHECK_EQUAL_COLLECTIONS(sPrepared.begin(), sPrepared.end(), sExpected.begin(), sExpected.end());

    BOOST_CHECK_EQUAL(sTest.next_serial(), 1);
    BOOST_CHECK_EQUAL(sTest.commit("file1:user1"), Store::Result::SUCCESS);
    BOOST_CHECK_EQUAL(sTest.status("file1:user1"), Txn::Status::COMMITED);
    BOOST_CHECK_EQUAL(sTest.next_serial(), 2);

    // commited transactions list
    std::vector<std::string> sCommited = sTest.list(Txn::Status::COMMITED); // list commited transactions
    BOOST_CHECK_EQUAL_COLLECTIONS(sCommited.begin(), sCommited.end(), sExpected.begin(), sExpected.end());

    // ensure data available after commit
    BOOST_CHECK_EQUAL(sTest.get("user1").value().data, "value1");
    BOOST_CHECK_EQUAL(sTest.get("user1").value().version, 1);

    // backup, clear, restore
    std::string sDump = sTest.backup();
    sTest.clear();
    sTest.restore(sDump);
    BOOST_CHECK_EQUAL(sTest.get("user1").value().data, "value1");
    BOOST_CHECK_EQUAL(sTest.get("user1").value().version, 1);

    // trim
    sTest.trim("file1:");
    BOOST_CHECK_EQUAL(sTest.status("file1:user1"), Txn::Status::UNKNOWN);

    // trim_pending
    BOOST_CHECK_EQUAL(
        sTest.prepare({.name      = "file1:user2",
                       .operation = TxnBody::Operation::INSERT,
                       .key       = "user2",
                       .value     = "value2"}),
        Store::Result::SUCCESS);
    BOOST_CHECK_EQUAL(sTest.status("file1:user2"), Txn::Status::PREPARE);
    sTest.trim_pending();
    BOOST_CHECK_EQUAL(sTest.status("file1:user2"), Txn::Status::UNKNOWN);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(manager)
BOOST_AUTO_TEST_CASE(simple)
{
    XA::Manager sMan;

    // create
    sMan.perform("file1:user1", "user1", [](auto& sExisting) { return "value1"; });
    BOOST_CHECK_EQUAL(sMan.get("user1").value().data, "value1");

    // update
    sMan.perform("file2:user1", "user1", [](auto& sExisting) { return "value2"; });
    BOOST_CHECK_EQUAL(sMan.get("user1").value().data, "value2");

    // delete
    sMan.perform("file3:user1", "user1", [](auto& sExisting) { return ""; });
    BOOST_CHECK_EQUAL(sMan.get("user1").has_value(), false);
}
BOOST_AUTO_TEST_SUITE_END() // manager