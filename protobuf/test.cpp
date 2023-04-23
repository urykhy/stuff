#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

// clang-format off
#include "Protobuf.hpp"
#include "Reflection.hpp"
// clang-format on

#include "ExprTK.hpp"
#include "tutorial.hpp"
#include "tutorial.pb.h"

#include <format/Hex.hpp>

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

    std::string_view sInput(sBuf);
    {
        Protobuf::Reader sReader(sInput);
        const auto       sTag1 = sReader.readTag();
        BOOST_CHECK_EQUAL(1, sTag1.id);
        sReader.skip(sTag1);
        BOOST_TEST_MESSAGE("tag1 " << (int)sTag1.tag);
        const auto sTag2 = sReader.readTag();
        BOOST_CHECK_EQUAL(2, sTag2.id);
        sReader.skip(sTag2);
        BOOST_TEST_MESSAGE("tag2 " << (int)sTag2.tag);
        const auto sTag3 = sReader.readTag();
        BOOST_CHECK_EQUAL(3, sTag3.id);
        sReader.skip(sTag3);
        BOOST_TEST_MESSAGE("tag3 " << (int)sTag3.tag);
        BOOST_CHECK(sReader.empty());
    }

    {
        MyPerson         sMyPerson;
        Protobuf::Reader sReader(sInput);
        sReader.parse([&sMyPerson](const Protobuf::FieldInfo& aField, Protobuf::Reader* aReader) -> Protobuf::Action {
            switch (aField.id) {
            case 1: return Protobuf::ACT_SKIP;
            case 2: aReader->read(sMyPerson.id); return Protobuf::ACT_USED;
            case 3: aReader->read(sMyPerson.email); return Protobuf::ACT_USED;
            }
            return Protobuf::ACT_BREAK;
        });
        BOOST_CHECK(sReader.empty());
        BOOST_CHECK_THROW(sReader.readTag(), Protobuf::EndOfBuffer);

        BOOST_CHECK(sMyPerson.name.empty());
        BOOST_CHECK_EQUAL(sMyPerson.id, sPerson.id());
        BOOST_CHECK_EQUAL(sMyPerson.email, sPerson.email());
    }

    // serialize to string
    {
        std::string      sTmp;
        Protobuf::Writer sWriter(sTmp);
        sWriter.write(1, "my_name");
        sWriter.write(2, 42);
        sWriter.write(3, "my_email");
        BOOST_CHECK_EQUAL(true, sPerson.ParseFromString(sTmp));
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

    std::string_view sInput(sBuf);
    Protobuf::Reader sReader(sInput);
    const auto       sTag1 = sReader.readTag();
    BOOST_CHECK_EQUAL(1, sTag1.id);
    sReader.skip(sTag1);
    const auto sTag2 = sReader.readTag();
    BOOST_CHECK_EQUAL(2, sTag2.id);
    BOOST_CHECK_EQUAL(sPerson.id(), sReader.readVarInt<int64_t>());
}
BOOST_AUTO_TEST_CASE(xdecode)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    auto xTest = [](auto p, auto v) {
        tutorial::xtest sMessage;
        p(sMessage);

        std::string sBuf;
        sMessage.SerializeToString(&sBuf);
        std::string_view sInput(sBuf);
        Protobuf::Reader sReader(sInput);
        v(sReader);
    };

    xTest([](auto& x) { x.set_i32(0); },
          [](auto& x) { BOOST_CHECK_EQUAL(1, x.readTag().id); uint32_t tmp = 0; x.read(tmp, Protobuf::FIXED); BOOST_CHECK_EQUAL(tmp, 0); });
    xTest([](auto& x) { x.set_i32(UINT32_MAX); },
          [](auto& x) { BOOST_CHECK_EQUAL(1, x.readTag().id); uint32_t tmp = 0; x.read(tmp, Protobuf::FIXED); BOOST_CHECK_EQUAL(tmp, UINT32_MAX); });

    xTest([](auto& x) { x.set_s32(0); },
          [](auto& x) { BOOST_CHECK_EQUAL(2, x.readTag().id); int32_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, 0); });
    xTest([](auto& x) { x.set_s32(-450); },
          [](auto& x) { BOOST_CHECK_EQUAL(2, x.readTag().id); int32_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, -450); });
    xTest([](auto& x) { x.set_s32(INT32_MAX); },
          [](auto& x) { BOOST_CHECK_EQUAL(2, x.readTag().id); int32_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, INT32_MAX); });
    xTest([](auto& x) { x.set_s32(INT32_MIN); },
          [](auto& x) { BOOST_CHECK_EQUAL(2, x.readTag().id); int32_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, INT32_MIN); });

    xTest([](auto& x) { x.set_f32(-4.6); },
          [](auto& x) { BOOST_CHECK_EQUAL(3, x.readTag().id); float tmp = 0; x.read(tmp); BOOST_CHECK_CLOSE(tmp, -4.6, 0.001); });

    xTest([](auto& x) { x.set_i64(0); },
          [](auto& x) { BOOST_CHECK_EQUAL(10, x.readTag().id); uint64_t tmp = 0; x.read(tmp, Protobuf::FIXED); BOOST_CHECK_EQUAL(tmp, 0); });
    xTest([](auto& x) { x.set_i64(UINT64_MAX); },
          [](auto& x) { BOOST_CHECK_EQUAL(10, x.readTag().id); uint64_t tmp = 0; x.read(tmp, Protobuf::FIXED); BOOST_CHECK_EQUAL(tmp, UINT64_MAX); });

    xTest([](auto& x) { x.set_s64(0); },
          [](auto& x) { BOOST_CHECK_EQUAL(11, x.readTag().id); int64_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, 0); });
    xTest([](auto& x) { x.set_s64(INT64_MAX); },
          [](auto& x) { BOOST_CHECK_EQUAL(11, x.readTag().id); int64_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, INT64_MAX); });
    xTest([](auto& x) { x.set_s64(INT64_MIN); },
          [](auto& x) { BOOST_CHECK_EQUAL(11, x.readTag().id); int64_t tmp = 0; x.read(tmp, Protobuf::ZIGZAG); BOOST_CHECK_EQUAL(tmp, INT64_MIN); });

    xTest([](auto& x) { x.set_f64(-4.6); },
          [](auto& x) { BOOST_CHECK_EQUAL(12, x.readTag().id); double tmp = 0; x.read(tmp); BOOST_CHECK_CLOSE(tmp, -4.6, 0.001); });
}
BOOST_AUTO_TEST_CASE(generated)
{
    tutorial::xtest sMessage;
    sMessage.set_i32(123);  // fixed
    sMessage.set_s32(-45);  // zz
    sMessage.set_f32(-4.6); // fixed float
    sMessage.set_f64(-6.2); // fixed float 64
    sMessage.add_packed_list(12);
    sMessage.add_packed_list(14);
    sMessage.set_binary("321");

    std::string sBuf;
    sMessage.SerializeToString(&sBuf);

    char                                sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::xtest                 sCustom(&sPool);
    sCustom.ParseFromString(sBuf);
    BOOST_CHECK_EQUAL(sMessage.i32(), *sCustom.i32);
    BOOST_CHECK_EQUAL(sMessage.s32(), *sCustom.s32);
    BOOST_CHECK_EQUAL(sMessage.f32(), *sCustom.f32);
    BOOST_CHECK_EQUAL(sMessage.f64(), *sCustom.f64);
    BOOST_CHECK_EQUAL(2, sCustom.packed_list.size());
    for (unsigned i = 0; i < sCustom.packed_list.size(); i++) {
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

    BOOST_TEST_MESSAGE("hex : " << Format::to_hex_c_string(sBuf));
    BOOST_TEST_MESSAGE("size: " << sBuf.size());

    char                                sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::Person                sCustom(&sPool);
    sCustom.ParseFromString(sBuf);
    BOOST_CHECK(!sCustom.m_Error);

    pmr_tutorial::PersonView sView(&sPool);
    sView.ParseFromString(sBuf);
    BOOST_CHECK(!sView.m_Error);

    // print in json
    BOOST_TEST_MESSAGE("json: " << Format::Json::to_string(sCustom.to_json()));
    BOOST_TEST_MESSAGE("view: " << Format::Json::to_string(sView.to_json()));
}

BOOST_AUTO_TEST_SUITE(reflection)
BOOST_AUTO_TEST_CASE(simple)
{
    char                                sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::xtest                 sCustom(&sPool);
    sCustom.i32 = 123;

    auto sKey = pmr_tutorial::xtest::GetReflectionKey("i32");
    BOOST_REQUIRE_NE(sKey, nullptr);
    BOOST_CHECK_EQUAL(sKey->id, 1);
    uint32_t sVal = 0;
    sCustom.GetByID(sKey->id, [&sVal](auto x) mutable {
        if constexpr (std::is_same_v<decltype(x), std::optional<uint32_t>>) {
            sVal = *x;
        }
    });
    BOOST_CHECK_EQUAL(sVal, 123);
}

BOOST_AUTO_TEST_CASE(walk)
{
    char                                sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};
    pmr_tutorial::rwalk                 sCustom(&sPool);

    sCustom.m1         = {};
    sCustom.m1->m2     = {};
    sCustom.m1->m2->id = 10;
    int32_t sVal       = 0;

    std::vector<uint32_t> sPath;
    Protobuf::Reflection<pmr_tutorial::rwalk>::walk(sCustom, "m1.m2.id", sPath);
    assert(sPath.size() == 3);

    Protobuf::Reflection<pmr_tutorial::rwalk>::use(sCustom, sPath, [&sVal](auto x) mutable {
        if constexpr (std::is_same_v<decltype(x), std::optional<int32_t>>) {
            sVal = *x;
        }
    });
    BOOST_CHECK_EQUAL(sVal, 10);
}
BOOST_AUTO_TEST_SUITE_END() // reflection

BOOST_AUTO_TEST_CASE(exprtk)
{
    char                                sBuffer[1024] = {};
    std::pmr::monotonic_buffer_resource sPool{std::data(sBuffer), std::size(sBuffer)};

    Protobuf::ExprTK sExpr;
    sExpr.m_Table.create_variable("m1.m2.id");
    sExpr.compile("m1.m2.id");

    pmr_tutorial::rwalk sVal(&sPool);
    sVal.m1         = pmr_tutorial::rwalk::xpart1(&sPool);
    sVal.m1->m2     = pmr_tutorial::rwalk::xpart2(&sPool);
    sVal.m1->m2->id = 10;

    sExpr.resolveFrom(sVal);
    sExpr.assignFrom(sVal);

    BOOST_CHECK_EQUAL(sExpr.eval(), 10);
}
BOOST_AUTO_TEST_SUITE_END() // Protobuf