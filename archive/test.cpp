#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <boost/mpl/list.hpp>

#include <file/File.hpp>
#include <format/Float.hpp>
#include <time/Meter.hpp>

#include "Bzip2.hpp"
#include "Gzip.hpp"
#include "LZ4.hpp"
#include "Util.hpp"
#include "XZ.hpp"
#include "Zstd.hpp"

template <class T>
std::string simpleFilter(const std::string& aStr)
{
    std::string sResult;
    std::string sBuffer(2, ' '); // dst buffer contain 2 bytes

    T      sFilter;
    size_t sInputPos = 0;
    while (sInputPos < aStr.size()) {
        auto sInputSize = std::min(aStr.size() - sInputPos, 2ul); // input upto 2 bytes
        auto sInfo      = sFilter.filter(aStr.data() + sInputPos, sInputSize, &sBuffer[0], sBuffer.size());
        sInputPos += sInfo.usedSrc;
        sResult.append(sBuffer.substr(0, sInfo.usedDst));
        if (sInfo.usedDst == 0 and sInfo.usedSrc == 0)
            throw std::logic_error("Archive::filter make no progress");
    }

    while (true) {
        auto sInfo = sFilter.finish(&sBuffer[0], sBuffer.size());
        sResult.append(sBuffer.substr(0, sInfo.usedDst));
        if (sInfo.done)
            break;
        if (sInfo.usedDst == 0)
            throw std::logic_error("Archive::filter make no progress");
    }

    return sResult;
}

struct LZ4
{
    using C = Archive::WriteLZ4;
    using D = Archive::ReadLZ4;
};
struct Zstd
{
    using C = Archive::WriteZstd;
    using D = Archive::ReadZstd;
};
struct XZ
{
    using C = Archive::WriteXZ;
    using D = Archive::ReadXZ;
};
struct Gzip
{
    using C = Archive::WriteGzip;
    using D = Archive::ReadGzip;
};
struct Bzip2
{
    using C = Archive::WriteBzip2;
    using D = Archive::ReadBzip2;
};

using FilterTypes = boost::mpl::list<LZ4, Zstd, XZ, Gzip, Bzip2>;

BOOST_AUTO_TEST_SUITE(Archive)
BOOST_AUTO_TEST_CASE_TEMPLATE(simple, T, FilterTypes)
{
    const std::string sData = File::to_string("/bin/bash");

    Time::Meter       sMeter;
    const std::string sCompressed   = simpleFilter<typename T::C>(sData);
    double            sCompressTime = sMeter.get().to_double();

    sMeter.reset();
    const std::string sClear     = simpleFilter<typename T::D>(sCompressed);
    double            sClearTime = sMeter.get().to_double();

    float sRatio = sData.size() / (float)sCompressed.size();
    BOOST_TEST_MESSAGE("\t compresed size: " << sCompressed.size());
    BOOST_TEST_MESSAGE("\t          ratio: " << Format::with_precision(sRatio, 2));
    BOOST_TEST_MESSAGE("\t  compress time: " << sCompressTime);
    BOOST_TEST_MESSAGE("\tdecompress time: " << sClearTime);

    BOOST_CHECK_EQUAL(sData.size(), sClear.size());
    BOOST_CHECK_EQUAL(sData == sClear, true);
}
BOOST_AUTO_TEST_CASE_TEMPLATE(concat, T, FilterTypes)
{
    const std::string s1 = "hello";
    const std::string s2 = " world";

    const std::string sC1 = simpleFilter<typename T::C>(s1);
    const std::string sC2 = simpleFilter<typename T::C>(s2);

    const std::string sClear = simpleFilter<typename T::D>(sC1 + sC2);
    BOOST_CHECK_EQUAL(sClear, "hello world");
}
BOOST_AUTO_TEST_CASE(ZstdThreads)
{
    const std::string sData = File::to_string("/bin/bash");
    BOOST_TEST_MESSAGE("source file size: " << sData.size());

    Zstd::C      sCompressor1(3);
    Time::Meter  sMeter;
    const auto   sC1    = Archive::filter(&sCompressor1, sData);
    const double sTime1 = sMeter.get().to_double();

    Zstd::C sCompressor2(3, 4); // use 4 threads
    sMeter.reset();
    const auto   sC2    = Archive::filter(&sCompressor2, sData);
    const double sTime2 = sMeter.get().to_double();

    BOOST_CHECK_CLOSE(float(sC1.size()), float(sC2.size()), 1);
    BOOST_TEST_MESSAGE("1 thread  time: " << sTime1);
    BOOST_TEST_MESSAGE("4 threads time: " << sTime2);
}
BOOST_AUTO_TEST_SUITE_END()
