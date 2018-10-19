#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <Protobuf.hpp>
#include "tutorial.pb.h"

struct MyPerson
{
    std::string name;
    std::string email;
    int32_t     id = 0;
};

BOOST_AUTO_TEST_SUITE(Protobuf)
BOOST_AUTO_TEST_CASE(simple)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	tutorial::Person sPerson;
	sPerson.set_email("foo@bar.com");
    sPerson.set_name("Kevin");
    sPerson.set_id(0x01020304);

    std::string sBuf;
    BOOST_CHECK_EQUAL(true, sPerson.SerializeToString(&sBuf));


    sPerson.Clear();
    BOOST_CHECK_EQUAL(sPerson.id(), 0);

    BOOST_CHECK_EQUAL(true, sPerson.ParseFromString(sBuf));
    BOOST_CHECK_EQUAL(sPerson.id(), 0x01020304);

    Protobuf::Buffer sInput(sBuf);
    {
        Protobuf::Walker sWalker(sInput);
        const auto sTag1 = sWalker.readTag(); BOOST_CHECK_EQUAL(1, sTag1.id); sWalker.skip(sTag1);
        const auto sTag2 = sWalker.readTag(); BOOST_CHECK_EQUAL(2, sTag2.id); sWalker.skip(sTag2);
        const auto sTag3 = sWalker.readTag(); BOOST_CHECK_EQUAL(3, sTag3.id); sWalker.skip(sTag3);
        BOOST_CHECK(sWalker.empty());
    }

    {
        MyPerson sMyPerson;
        Protobuf::Walker sWalker(sInput);
        sWalker.parse([&sMyPerson](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker){
            switch (aField.id)
            {
            case 1: aWalker->skip(aField);  break;
            case 2: sMyPerson.id    = aWalker->readVarUInt(); break;
            case 3: sMyPerson.email = aWalker->readString();  break;
            }
        });
        BOOST_CHECK(sWalker.empty());
        BOOST_CHECK_THROW(sWalker.readTag(), Protobuf::EndOfBuffer);

        BOOST_CHECK(sMyPerson.name.empty());
        BOOST_CHECK_EQUAL(sMyPerson.id,    sPerson.id());
        BOOST_CHECK_EQUAL(sMyPerson.email, sPerson.email());
    }
}
BOOST_AUTO_TEST_CASE(sint)
{
    // check signed integer decoder
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	tutorial::Person sPerson;
    sPerson.set_name("dummy");
    sPerson.set_id(-123456);

    std::string sBuf;
    sPerson.SerializeToString(&sBuf);

    Protobuf::Buffer sInput(sBuf);
    Protobuf::Walker sWalker(sInput);
    const auto sTag1 = sWalker.readTag(); BOOST_CHECK_EQUAL(1, sTag1.id); sWalker.skip(sTag1);
    const auto sTag2 = sWalker.readTag(); BOOST_CHECK_EQUAL(2, sTag2.id); BOOST_CHECK_EQUAL(sPerson.id(), sWalker.readVarInt());
}
BOOST_AUTO_TEST_SUITE_END()
