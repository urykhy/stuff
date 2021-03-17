#pragma once
#include <endian.h>
#include <string.h>

#include <cassert>
#include <cstdint>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "file/Reader.hpp"
#include "file/Writer.hpp"

#include <container/Stream.hpp>

namespace cbor {
    using binary = std::string;

    enum
    {
        CBOR_UINT   = 0,
        CBOR_NINT   = 1,
        CBOR_BINARY = 2,
        CBOR_STR    = 3,
        CBOR_LIST   = 4,
        CBOR_MAP    = 5,
        CBOR_TAG    = 6,
        CBOR_X      = 7,

        CBOR_FALSE  = 20,
        CBOR_TRUE   = 21,
        CBOR_NULL   = 22,
        CBOR_8      = 24,
        CBOR_16     = 25,
        CBOR_32     = 26,
        CBOR_64     = 27,
        CBOR_FLOAT  = 26,
        CBOR_DOUBLE = 27,
    };

    using istream = File::IExactReader;
    using ostream = File::IWriter;

    using imemstream = Container::imemstream;
    using omemstream = Container::omemstream;

    // IMPL DETAILS

    template <class T>
    typename std::enable_if<sizeof(T) == 1, T>::type be2h(T val) { return val; }
    template <class T>
    typename std::enable_if<sizeof(T) == 2, T>::type be2h(T val) { return be16toh(val); }
    template <class T>
    typename std::enable_if<sizeof(T) == 4, T>::type be2h(T val) { return be32toh(val); }
    template <class T>
    typename std::enable_if<sizeof(T) == 8, T>::type be2h(T val) { return be64toh(val); }

    template <class T>
    auto read_integer(istream& s)
    {
        T t;
        s.read(&t, sizeof(t));
        t = be2h(t);
        return t;
    }

    template <class T = uint64_t>
    T get_uint(istream& s, uint8_t minorType)
    {
        if (minorType < CBOR_8)
            return minorType;
        if (minorType == CBOR_8)
            return read_integer<uint8_t>(s);
        if (minorType == CBOR_16)
            return read_integer<uint16_t>(s);
        if (minorType == CBOR_32)
            return read_integer<uint32_t>(s);
        if (minorType == CBOR_64)
            return read_integer<uint64_t>(s);
        throw std::invalid_argument("unexpected minor type: " + std::to_string(minorType));
    }

    struct TypeInfo
    {
        uint8_t minor : 5; // size. (type & 31)
        uint8_t major : 3; // type. (type >> 5)
    };

    inline TypeInfo get_type(istream& s)
    {
        TypeInfo info = {};
        while (1) {
            s.read(&info, sizeof(info));
            if (info.major == CBOR_TAG) {
                get_uint(s, info.minor);
                continue;
            }
            break;
        }
        return info;
    }

    inline uint8_t ensure_type(istream& s, uint8_t needType)
    {
        TypeInfo t = get_type(s);
        if (t.major != needType)
            throw std::invalid_argument("unexpected major type: " + std::to_string(t.major));
        return t.minor;
    }

    inline void write_type_value(ostream& out, uint8_t major_type, uint64_t value)
    {
        major_type <<= 5;
        if (value < CBOR_8) {
            uint8_t sByte = major_type | value;
            out.write(&sByte, 1);
        } else if (value <= __UINT8_MAX__) {
            uint8_t sByte = major_type | CBOR_8;
            out.write(&sByte, 1);
            sByte = value;
            out.write(&sByte, 1);
        } else if (value <= __UINT16_MAX__) {
            uint8_t sByte = major_type | CBOR_16;
            out.write(&sByte, 1);
            uint16_t tmp = htobe16((uint16_t)value);
            out.write(&tmp, sizeof(tmp));
        } else if (value <= __UINT32_MAX__) {
            uint8_t sByte = major_type | CBOR_32;
            out.write(&sByte, 1);
            uint32_t tmp = htobe32((uint32_t)value);
            out.write(&tmp, sizeof(tmp));
        } else {
            uint8_t sByte = major_type | CBOR_64;
            out.write(&sByte, 1);
            uint64_t tmp = htobe64(value);
            out.write(&tmp, sizeof(tmp));
        }
    }

    inline void write_special(ostream& out, uint8_t special)
    {
        uint8_t sByte = (uint8_t)(CBOR_X << 5 | special);
        out.write(&sByte, 1);
    }
} // namespace cbor