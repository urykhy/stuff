#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include "Bloom.hpp"
#include <file/File.hpp>

// data.txt from https://github.com/daank94/bloomfilter/tree/master/data

BOOST_AUTO_TEST_SUITE(Bloom)
BOOST_AUTO_TEST_CASE(basic)
{
    Bloom::Set sFilter(4 * 1024);
    std::vector<std::string> sStrings;

    File::by_string("../data.txt", [&sStrings](auto&& s){
        sStrings.push_back(std::move(s));
    });
    BOOST_CHECK_EQUAL(sStrings.size(), 5000);

    std::vector<std::string> sMiss;
    for (unsigned i = 0; i < 500; i++)
        sFilter.insert(Bloom::hash(sStrings[i]));
    for (unsigned i = 500; i < sStrings.size(); i++)
        if (sFilter.test(Bloom::hash(sStrings[i])))
            sMiss.push_back(sStrings[i]);
    BOOST_TEST_MESSAGE("found " << sMiss.size() << " false hits across " << sStrings.size() << " strings");
    BOOST_TEST_MESSAGE("estimated false positive: " << Bloom::estimate(500, 4 * 1024, 4) * sStrings.size());

    // 2nd filter
    // store only false-positives from 1st filter
    Bloom::Set sSmall(2 * 1024 + 128);
    for (auto& x : sMiss)
        sSmall.insert(Bloom::hash(x));

    // check large bloom, and ensure miss for small one
    auto test2 = [&](const auto aStr) -> bool
    {
        const auto hash = Bloom::hash(aStr);
        return sFilter.test(hash) and !sSmall.test(hash);
    };

    unsigned sMiss2 = 0;
    for (unsigned i = 0; i < 500; i++)
        if (!test2(sStrings[i]))
            sMiss2++;
    for (unsigned i = 500; i < sStrings.size(); i++)
        if (test2(sStrings[i]))
            sMiss2++;
    BOOST_TEST_MESSAGE("found " << sMiss2 << " false hits across " << sStrings.size() << " strings");

    // (1024 * 6 + 128)/5000 = 1.25 bit per string
}
BOOST_AUTO_TEST_SUITE_END()