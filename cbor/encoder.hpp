#pragma once
#include "cbor-internals.hpp"

namespace cbor {

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

    // API

    template <class T>
    typename std::enable_if<
        std::is_signed<T>::value, void>::type
    write(ostream& out, T value)
    {
        if (value < 0) {
            write_type_value(out, CBOR_NINT, (uint64_t) - (value + 1));
        } else {
            write_type_value(out, CBOR_UINT, (uint64_t)value);
        }
    }

    template <class T>
    typename std::enable_if<
        std::is_unsigned<T>::value, void>::type
    write(ostream& out, T value)
    {
        write_type_value(out, CBOR_UINT, value);
    }

    template <class T>
    typename std::enable_if<
        std::is_trivial<T>::value, void>::type
    write(ostream& out, const std::vector<T>& data)
    {
        write_type_value(out, CBOR_BINARY, data.size() * sizeof(T));
        out.write(data.data(), data.size() * sizeof(T));
    }

    inline void write(ostream& out, const std::string_view& str)
    {
        write_type_value(out, CBOR_STR, str.size());
        out.write(str.data(), str.size());
    }

    inline void write(ostream& out, const char* str)
    {
        auto len = strlen(str);
        write_type_value(out, CBOR_STR, len);
        out.write(str, len);
    }

    inline void write(ostream& out, float v)
    {
        write_special(out, CBOR_FLOAT);
        union
        {
            float    f;
            uint32_t u;
        } un;
        un.f = v;
        un.u = htobe32(un.u);
        out.write(&un.u, sizeof(un.u));
    }
    inline void write(ostream& out, double v)
    {
        write_special(out, CBOR_DOUBLE);
        union
        {
            double   d;
            uint64_t u;
        } un;
        un.d = v;
        un.u = htobe64(un.u);
        out.write(&un.u, sizeof(un.u));
    }
    inline void write(ostream& out, bool v)
    {
        write_special(out, v ? CBOR_TRUE : CBOR_FALSE);
    }

    template <class T>
    void write(ostream& out, const std::list<T>& l)
    {
        write_type_value(out, CBOR_LIST, l.size());
        for (const auto& x : l) {
            write(out, x);
        }
    }

    template <class T>
    void write(ostream& out, const std::vector<T>& l)
    {
        write_type_value(out, CBOR_LIST, l.size());
        for (const auto& x : l) {
            write(out, x);
        }
    }

    template <class K, class V>
    void write(ostream& out, const std::map<K, V>& map)
    {
        write_type_value(out, CBOR_MAP, map.size());
        for (const auto& x : map) {
            write(out, x.first);
            write(out, x.second);
        }
    }

    inline void write_tag(ostream& out, uint64_t tag)
    {
        write_type_value(out, CBOR_TAG, tag);
    }

} // namespace cbor