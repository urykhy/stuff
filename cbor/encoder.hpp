#pragma once
#include "cbor-internals.hpp"

namespace cbor {

    inline void write_type_value(omemstream& out, uint8_t major_type, uint64_t value)
    {
        major_type <<= 5;
        if(value < 24ULL) {
            out.put(major_type | value);
        } else if(value <= __UINT8_MAX__) {
            out.put(major_type | CBOR_8);
            out.put((uint8_t) value);
        } else if(value <= __UINT16_MAX__) {
            out.put(major_type | CBOR_16);
            uint16_t tmp = htobe16((uint16_t)value);
            out.write(&tmp, sizeof(tmp));
        } else if(value <= __UINT32_MAX__) {
            out.put(major_type | CBOR_32);
            uint32_t tmp = htobe32((uint32_t)value);
            out.write(&tmp, sizeof(tmp));
        } else {
            out.put(major_type | CBOR_64);
            uint64_t tmp = htobe64(value);
            out.write(&tmp, sizeof(tmp));
        }
    }

    inline void write_special(omemstream& out, uint8_t special) {
        out.put((uint8_t) ( 7 << 5 | special));
    }

    // API

    template<class T>
    typename std::enable_if<
        std::is_signed<T>::value, void
    >::type
    write(omemstream& out, T value) {
        if(value < 0) {
            write_type_value(out, CBOR_NINT, (uint64_t) -(value+1));
        } else {
            write_type_value(out, CBOR_UINT, (uint64_t) value);
        }
    }

    template<class T>
    typename std::enable_if<
        std::is_unsigned<T>::value, void
    >::type
    write(omemstream& out, T value) {
        write_type_value(out, CBOR_UINT, value);
    }

    template<class T>
    typename std::enable_if<
        std::is_trivial<T>::value, void
    >::type
    write(omemstream& out, const std::vector<T>& data) {
        write_type_value(out, CBOR_BINARY, data.size() * sizeof(T));
        out.write(data.data(), data.size() * sizeof(T));
    }

    inline void
    write(omemstream& out, const binary_ref& data) {
        write_type_value(out, CBOR_BINARY, boost::asio::buffer_size(data));
        out.write(boost::asio::buffer_cast<const void*>(data), boost::asio::buffer_size(data));
    }

    inline void write(omemstream& out, const boost::string_ref& str) {
        write_type_value(out, CBOR_STR, str.size());
        out.write(str.data(), str.size());
    }

    inline void write(omemstream& out, const char* str) {
        auto len = strlen(str);
        write_type_value(out, CBOR_STR, len);
        out.write(str, len);
    }

    inline void write(omemstream& out, float v) {
        write_special(out, CBOR_FLOAT);
        union {
            float f;
            uint32_t u;
        } un;
        un.f = v;
        un.u = htobe32(un.u);
        out.write(&un.u, sizeof(un.u));
    }
    inline void write(omemstream& out, double v) {
        write_special(out, CBOR_DOUBLE);
        union {
            double d;
            uint64_t u;
        } un;
        un.d = v;
        un.u = htobe64(un.u);
        out.write(&un.u, sizeof(un.u));
    }
    inline void write(omemstream& out, bool v) {
        write_special(out, v ? CBOR_TRUE : CBOR_FALSE);
    }

    template<class T>
    void write(omemstream& out, const std::list<T>& l)
    {
        write_type_value(out, CBOR_LIST, l.size());
        for (const auto& x : l) {
            write(out, x);
        }
    }

    template<class K, class V>
    void write(omemstream& out, const std::map<K, V>& map)
    {
        write_type_value(out, CBOR_MAP, map.size());
        for (const auto& x : map) {
            write(out, x.first);
            write(out, x.second);
        }
    }

    inline void write_tag(omemstream& out, uint64_t tag) {
        write_type_value(out, CBOR_TAG, tag);
    }

} // namespace