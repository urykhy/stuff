#define BOOST_TEST_MODULE Suites

#include <boost/test/unit_test.hpp>

#include <string>

#include "Small.hpp"

BOOST_AUTO_TEST_SUITE(allocator)
BOOST_AUTO_TEST_CASE(pool)
{
    struct Data : Allocator::SmallObject<Data, Allocator::Pool<Data>>
    {
        char tmp[12];
    };
    Data::Allocator sAllocator;
    Allocator::Guard<Data::Allocator> sGuard(&sAllocator);

    Data* n = new Data;
    BOOST_CHECK_EQUAL(sAllocator.size(), 4096);
    BOOST_CHECK_EQUAL(sAllocator.avail(), 4095);
    delete n;
    BOOST_CHECK_EQUAL(sAllocator.avail(), 4096);

    n = new Data;
    BOOST_CHECK_EQUAL(sAllocator.avail(), 4095);
    delete n;
}
BOOST_AUTO_TEST_CASE(arena)
{
    using A = Allocator::Face<char, Allocator::Arena>;
    struct Data : Allocator::SmallObject<Data, A>
    {
        char tmp[12];
    };
    Data::Allocator sAllocator; // Arena
    Allocator::Guard<Data::Allocator> sGuard(&sAllocator);

    Data* n = new Data;
    BOOST_CHECK_EQUAL(sAllocator.max_size(), Data::Allocator::SIZE - sizeof(Data));
    delete n;
    BOOST_CHECK_EQUAL(sAllocator.max_size(), Data::Allocator::SIZE - sizeof(Data)); // size not returned
    sAllocator.clear();
    BOOST_CHECK_EQUAL(sAllocator.max_size(), Data::Allocator::SIZE);
}
BOOST_AUTO_TEST_CASE(face)
{
    using A = Allocator::Face<char, Allocator::Arena>;
    Allocator::Arena sAllocator;
    Allocator::Guard<Allocator::Arena> sGuard(&sAllocator);

    using Str = std::basic_string<char, std::char_traits<char>, A>;
    Str sTmp("123456789012345678901234567890"); // no need to pass allocator
    BOOST_CHECK(sAllocator.max_size() < Allocator::Arena::SIZE);
    BOOST_CHECK(sAllocator.used() == sTmp.size() + 1);
}
BOOST_AUTO_TEST_SUITE_END()
