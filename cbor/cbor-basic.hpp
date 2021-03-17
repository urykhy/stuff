#pragma once

#include "cbor-internals.hpp"

namespace cbor {

    // unsigned int
    template <class T>
    typename std::enable_if<std::is_unsigned<T>::value, void>::type
    read(istream& s, T& val)
    {
        val = get_uint<T>(s, ensure_type(s, CBOR_UINT));
    }

    template <class T>
    typename std::enable_if<std::is_unsigned<T>::value, void>::type
    write(ostream& out, T value)
    {
        write_type_value(out, CBOR_UINT, value);
    }

    // signed int
    template <class T>
    typename std::enable_if<std::is_signed<T>::value, void>::type
    read(istream& s, T& val)
    {
        const auto t = get_type(s);
        switch (t.major) {
        case CBOR_UINT: val = get_uint<T>(s, t.minor); return;
        case CBOR_NINT: val = -1 - get_uint<T>(s, t.minor); return;
        default: throw std::invalid_argument("unexpected major type: " + std::to_string(t.major));
        }
    }

    template <class T>
    typename std::enable_if<std::is_signed<T>::value, void>::type
    write(ostream& out, T value)
    {
        if (value < 0) {
            write_type_value(out, CBOR_NINT, (uint64_t) - (value + 1));
        } else {
            write_type_value(out, CBOR_UINT, (uint64_t)value);
        }
    }

    // float
    inline void read(istream& s, float& v)
    {
        auto minorType = ensure_type(s, CBOR_X);
        if (minorType == CBOR_FLOAT) {
            union
            {
                float    f;
                uint32_t u;
            } un;
            s.read(&un.u, sizeof(un.u));
            un.u = be2h(un.u);
            v    = un.f;
            return;
        }
        throw std::invalid_argument("unexpected special: " + std::to_string(minorType));
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

    // double
    inline void read(istream& s, double& v)
    {
        auto minorType = ensure_type(s, CBOR_X);
        if (minorType == CBOR_DOUBLE) {
            union
            {
                double   d;
                uint64_t u;
            } un;
            s.read(&un.u, sizeof(un.u));
            un.u = be2h(un.u);
            v    = un.d;
            return;
        }
        throw std::invalid_argument("unexpected special:" + std::to_string(minorType));
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

    // bool
    inline void read(istream& s, bool& v)
    {
        auto minorType = ensure_type(s, CBOR_X);
        switch (minorType) {
        case CBOR_FALSE: v = false; return;
        case CBOR_TRUE: v = true; return;
        default: throw std::invalid_argument("unexpected special: " + std::to_string(minorType));
        }
    }

    inline void write(ostream& out, bool v)
    {
        write_special(out, v ? CBOR_TRUE : CBOR_FALSE);
    }

    // tag
    inline bool read_tag(istream& s, uint64_t& tag)
    {
        TypeInfo info = {};
        s.read(&info, sizeof(info));
        if (info.major == CBOR_TAG) {
            tag = get_uint(s, info.minor);
            return true;
        }
        s.unget();
        return false;
    }

    inline void write_tag(ostream& out, uint64_t tag)
    {
        write_type_value(out, CBOR_TAG, tag);
    }
} // namespace cbor