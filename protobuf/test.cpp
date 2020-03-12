#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <Protobuf.hpp>
#include "tutorial.pb.h"

#include <../parser/Hex.hpp>
#include "tutorial.hpp"

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
        sWalker.parse([&sMyPerson](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker) -> Protobuf::Action {
            switch (aField.id)
            {
                case 1: return Protobuf::ACT_SKIP;
                case 2: sMyPerson.id    = aWalker->readVarInt<int>(); return Protobuf::ACT_USED;
                case 3: sMyPerson.email = aWalker->readString();  return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
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
    const auto sTag2 = sWalker.readTag(); BOOST_CHECK_EQUAL(2, sTag2.id); BOOST_CHECK_EQUAL(sPerson.id(), sWalker.readVarInt<int64_t>());
}
BOOST_AUTO_TEST_CASE(xdecode)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    tutorial::xtest sMessage;
    sMessage.set_i32(123);  // fixed
    sMessage.set_s32(-45);  // zz
    sMessage.set_f32(-4.6); // fixed float

    std::string sBuf;
    sMessage.SerializeToString(&sBuf);

    Protobuf::Buffer sInput(sBuf);
    Protobuf::Walker sWalker(sInput);

    uint32_t i32 = 0;
    int32_t  s32 = 0;
    float    f32 = 0;
    const auto sTag1 = sWalker.readTag(); BOOST_CHECK_EQUAL(1, sTag1.id); sWalker.read(i32, Protobuf::Walker::FIXED); BOOST_CHECK_EQUAL(sMessage.i32(), i32);
    const auto sTag2 = sWalker.readTag(); BOOST_CHECK_EQUAL(2, sTag2.id); sWalker.read(s32, Protobuf::Walker::ZIGZAG); BOOST_CHECK_EQUAL(sMessage.s32(), s32);
    const auto sTag3 = sWalker.readTag(); BOOST_CHECK_EQUAL(3, sTag3.id); sWalker.read(f32); BOOST_CHECK_EQUAL(sMessage.f32(), f32);
}
BOOST_AUTO_TEST_CASE(generated)
{
    tutorial::xtest sMessage;
    sMessage.set_i32(123);  // fixed
    sMessage.set_s32(-45);  // zz
    sMessage.set_f32(-4.6); // fixed float
    sMessage.add_packed_list(12);
    sMessage.add_packed_list(14);
    sMessage.set_binary("321");

    std::string sBuf;
    sMessage.SerializeToString(&sBuf);

    char sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::xtest sCustom(&sPool);
    sCustom.ParseFromString(sBuf);
    BOOST_CHECK_EQUAL(sMessage.i32(), *sCustom.i32);
    BOOST_CHECK_EQUAL(sMessage.s32(), *sCustom.s32);
    BOOST_CHECK_EQUAL(sMessage.f32(), *sCustom.f32);
    BOOST_CHECK_EQUAL(2, sCustom.packed_list.size());
    for (unsigned i = 0; i < sCustom.packed_list.size(); i++)
    {
        auto sIt = sCustom.packed_list.begin();
        std::advance(sIt, i);
        BOOST_CHECK_EQUAL(sMessage.packed_list(i), *sIt);
    }
    BOOST_CHECK_EQUAL(sMessage.binary(), sCustom.binary->c_str());
}
BOOST_AUTO_TEST_CASE(person)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    tutorial::Person sPerson;
    sPerson.set_email("foo@bar.com");
    sPerson.set_name("Kevin");
    sPerson.set_id(0x01020304);
    auto phone = sPerson.add_phones();
    phone->set_number("+1234567890");
    phone->set_type(tutorial::Person_PhoneType::Person_PhoneType_MOBILE);
    phone = sPerson.add_phones();
    phone->set_number("+0987654321");
    phone->set_type(tutorial::Person_PhoneType::Person_PhoneType_HOME);

    std::string sBuf;
    BOOST_CHECK_EQUAL(true, sPerson.SerializeToString(&sBuf));

    BOOST_TEST_MESSAGE("hex : " << Parser::to_hex_c_string(sBuf));
    BOOST_TEST_MESSAGE("size: " << sBuf.size());

    char sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::Person sCustom(&sPool);
    sCustom.ParseFromString(sBuf);

    // print in json
    Json::FastWriter sWriter;
    BOOST_TEST_MESSAGE("json: " << sWriter.write(sCustom.toJson()));
}
BOOST_AUTO_TEST_SUITE_END()
