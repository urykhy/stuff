#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Lite.hpp"

BOOST_AUTO_TEST_SUITE(SqlLite)
BOOST_AUTO_TEST_CASE(simple)
{
    Lite::DB sDB;
    sDB.Query("SELECT SQLITE_VERSION()", [](int aColumns, char const* const* aValues, char const* const* aNames) {
        BOOST_CHECK_EQUAL(aColumns, 1);
        BOOST_TEST_MESSAGE("version: " << aValues[0]);
        BOOST_CHECK_EQUAL(aNames[0], "SQLITE_VERSION()");
    });
    sDB.Query("CREATE TABLE IF NOT EXISTS users(Id INTEGER PRIMARY KEY, Name TEXT)");
    sDB.Query("INSERT INTO users(Name) VALUES ('Tom'), ('Jerry')");
    BOOST_CHECK_EQUAL(sDB.LastInsertRowid(), 2);

    sDB.Query("SELECT * FROM users ORDER BY Id", [sRow = 0](int aColumns, char const* const* aValues, char const* const* aNames) mutable {
        BOOST_REQUIRE_EQUAL(aColumns, 2);
        switch (sRow) {
        case 0:
            BOOST_CHECK_EQUAL(aValues[0], "1");
            BOOST_CHECK_EQUAL(aValues[1], "Tom");
            break;
        case 1:
            BOOST_CHECK_EQUAL(aValues[0], "2");
            BOOST_CHECK_EQUAL(aValues[1], "Jerry");
            break;
        }
        sRow++;
    });

    sDB.Query("SELECT type, name, sql FROM sqlite_schema", [](int aColumns, char const* const* aValues, char const* const* aNames) mutable {
        BOOST_REQUIRE_EQUAL(aColumns, 3);
        BOOST_TEST_MESSAGE(aValues[0] << '\t' << aValues[1] << '\t' << aValues[2]);
    });

    sDB.Query("PRAGMA journal_mode", [](int aColumns, char const* const* aValues, char const* const* aNames) mutable {
        BOOST_TEST_MESSAGE("journal_mode: " << aValues[0]);
    });

    // backup
    {
        Lite::DB sBackup("__test.backup.db");
        BOOST_CHECK_NO_THROW(sDB.Backup(sBackup));

        sBackup.Query("SELECT Name FROM users WHERE Id = 1", [](int aColumns, char const* const* aValues, char const* const* aNames) mutable {
            BOOST_REQUIRE_EQUAL(aColumns, 1);
            BOOST_CHECK_EQUAL(aNames[0], "Name");
            BOOST_CHECK_EQUAL(aValues[0], "Tom");
        });
    }

    // prepared
    unsigned sCalls = 0;
    {
        auto sCall = sDB.Prepare("SELECT Name FROM users WHERE Id = ?");
        sCall.Assign(1);
        sCall.Use([&sCalls](unsigned aColumns, char const* const* aValues, char const* const* aNames) {
            BOOST_REQUIRE_EQUAL(aColumns, 1);
            BOOST_CHECK_EQUAL(aNames[0], "Name");
            BOOST_CHECK_EQUAL(aValues[0], "Tom");
            sCalls++;
        });
    }
    {
        auto sCall = sDB.Prepare("SELECT Id FROM users WHERE Name = ?");
        sCall.Assign(std::string_view("Jerry"));
        sCall.Use([&sCalls](unsigned aColumns, char const* const* aValues, char const* const* aNames) {
            BOOST_REQUIRE_EQUAL(aColumns, 1);
            BOOST_CHECK_EQUAL(aNames[0], "Id");
            BOOST_CHECK_EQUAL(aValues[0], "2");
            sCalls++;
        });
    }
    BOOST_CHECK_EQUAL(sCalls, 2);
}
BOOST_AUTO_TEST_CASE(attach)
{
    Lite::DB sDB("__test.backup.db");
    sDB.Query("ATTACH DATABASE ':memory:' AS tmp");
    sDB.Query("CREATE TABLE IF NOT EXISTS tmp.flags(Key TEXT, Value TEXT)");

    sDB.Query("SELECT type, name, sql FROM tmp.sqlite_schema", [](int aColumns, char const* const* aValues, char const* const* aNames) mutable {
        BOOST_REQUIRE_EQUAL(aColumns, 3);
        BOOST_TEST_MESSAGE(aValues[0] << '\t' << aValues[1] << '\t' << aValues[2]);
    });
}
BOOST_AUTO_TEST_SUITE_END()
