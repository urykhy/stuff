#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#include "fakeit.hpp" // https://github.com/eranpeer/FakeIt/wiki/Quickstart
#pragma GCC diagnostic pop

struct SomeInterface {
   virtual int foo(int) = 0;
   virtual ~SomeInterface() {}
};

BOOST_AUTO_TEST_SUITE(mock)
BOOST_AUTO_TEST_CASE(simple)
{
    using namespace fakeit;

    Mock<SomeInterface> sMock;
    When(Method(sMock,foo).Using(1)).Return(1);
    When(Method(sMock,foo).Using(2)).Return(1).Return(2).Return(3);
    When(Method(sMock,foo).Using(3)).Throw(std::runtime_error("foo:3"));

    BOOST_CHECK_EQUAL(sMock.get().foo(1), 1);
    Verify(Method(sMock,foo)).Exactly(1);

    BOOST_CHECK_EQUAL(sMock.get().foo(2), 1);
    BOOST_CHECK_EQUAL(sMock.get().foo(2), 2);
    BOOST_CHECK_EQUAL(sMock.get().foo(2), 3);
    //Verify(Method(sMock,foo)).Exactly(3); // must be error here

    BOOST_CHECK_THROW(sMock.get().foo(3), std::runtime_error);

    // invocation order
    Verify(Method(sMock,foo).Using(1), Method(sMock,foo).Using(2), Method(sMock,foo).Using(2), Method(sMock,foo).Using(2), Method(sMock,foo).Using(3));
    VerifyNoOtherInvocations(sMock);
}
BOOST_AUTO_TEST_SUITE_END()