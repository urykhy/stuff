#pragma once

#include <string_view>

#include <boost/crc.hpp>

#include <mpl/Mpl.hpp>

namespace Hash::CRC32 {

    // zlib CRC-32 (ITU-T V.42)
    // https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat-bits.32
    using SUM = boost::crc_optimal<32, 0x04c11db7, 0xffffffff, 0xffffffff, true, true>;

    namespace {
        inline void updateSum(SUM& aSum, std::string_view aStr)
        {
            aSum.process_bytes(aStr.data(), aStr.size());
        }
        template <class T>
        typename std::enable_if<std::is_integral_v<T>, void>::type updateSum(SUM& aSum, const T& aInput)
        {
            aSum.process_bytes(&aInput, sizeof(aInput));
        }

    } // namespace

    template <class... T>
    uint32_t sum(T&&... aInput)
    {
        SUM sSum;
        Mpl::for_each_argument(
            [&](const auto& aInput) {
                updateSum(sSum, aInput);
            },
            aInput...);
        return sSum.checksum();
    }

} // namespace Hash::CRC32