#define BOOST_TEST_MODULE Suites

#include <boost/test/unit_test.hpp>

#include "Entity.hpp"

BOOST_AUTO_TEST_SUITE(ecs)
BOOST_AUTO_TEST_CASE(entity)
{
    struct C1
    {};
    struct C2
    {
        int m_Data = 0;
        C2(int aData)
        : m_Data(aData)
        {}
    };
    using E = ECS::Entity<C1, C2>;

    auto e = E::create();
    e.assign(C2{42});
    BOOST_CHECK_MESSAGE(e.get<C2>().value()->m_Data == 42, "get component value");
    e.erase<C2>();
    BOOST_CHECK_MESSAGE(!e.get<C2>(), "get nx component value");

    e.assign(C1{});
    bool sWithC1 = false;
    e.inspect(Mpl::overloaded{[&sWithC1](C1& x) mutable { sWithC1 = true; },
                              [](auto& x) { BOOST_CHECK_MESSAGE(false, "called with unexpected type " << typeid(x).name()); }});
    BOOST_CHECK_MESSAGE(sWithC1, "component1 attached");

    auto& sSystem  = E::system();
    int   sCountC1 = 0;
    sSystem.forEach<C1>([&sCountC1](auto&& aEntity, C1& aData) {
        BOOST_TEST_MESSAGE("entity " << aEntity.getID());
        BOOST_CHECK_MESSAGE(!aEntity.template get<C2>(), "get nx component value");
        sCountC1++;
    });
    BOOST_CHECK_MESSAGE(sCountC1 == 1, "number of C1 components");
}
BOOST_AUTO_TEST_SUITE_END()
