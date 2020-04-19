#pragma once
#include "cbor-internals.hpp"

namespace cbor {

    template<class T> typename std::enable_if<sizeof(T) == 1, T>::type be2h(T val) { return val; }
    template<class T> typename std::enable_if<sizeof(T) == 2, T>::type be2h(T val) { return be16toh(val); }
    template<class T> typename std::enable_if<sizeof(T) == 4, T>::type be2h(T val) { return be32toh(val); }
    template<class T> typename std::enable_if<sizeof(T) == 8, T>::type be2h(T val) { return be64toh(val); }

    template<class T> struct read_integer
    {
        static T read(imemstream& s)
        {
            T t;
            s.read(&t, sizeof(t));
            t = be2h(t);
            return t;
        }
    };

    template<class T = uint64_t>
    T get_uint(imemstream& s, uint8_t minorType)
    {
        if (minorType <  CBOR_8)  { return minorType; }
        if (minorType == CBOR_8)  { return read_integer<uint8_t>::read(s); }
        if (minorType == CBOR_16) { return read_integer<uint16_t>::read(s); }
        if (minorType == CBOR_32) { return read_integer<uint32_t>::read(s); }
        if (minorType == CBOR_64) { return read_integer<uint64_t>::read(s); }
        throw std::runtime_error("invalid integer type");
    }

    struct TypeInfo
    {
        uint8_t minor : 5;  // size. (type & 31)
        uint8_t major : 3;  // type. (type >> 5)
    };

    inline TypeInfo get_type(imemstream& s, const bool skip_tags = true) {
        TypeInfo info{0};
        do {
            s.read(&info, sizeof(info));
            if (skip_tags && info.major == CBOR_TAG) {   // skip unexpected tags
                get_uint(s, info.minor);
            }
        } while (skip_tags && info.major == CBOR_TAG);
        return info;
    }

    inline uint8_t ensure_type(imemstream& s, uint8_t needType)
    {
        TypeInfo t = get_type(s);
        //BOOST_TEST_MESSAGE("ensure type " << (int)t.first << " vs required " << (int)needType << ", special: " << (int)t.second);
        if (t.major != needType) {
            throw std::runtime_error("unexpected type");
        }
        return t.minor;
    }

    // only positive int
    template<class T>
    typename std::enable_if<
        std::is_unsigned<T>::value, void
    >::type
    read(imemstream& s, T& val) {
        val = get_uint<T>(s, ensure_type(s, CBOR_UINT));
    }

    // positive + negative integer
    template<class T>
    typename std::enable_if<
        std::is_signed<T>::value, void
    >::type
    read(imemstream& s, T& val) {
        const auto t = get_type(s);
        switch (t.major) {
            case CBOR_UINT: val = get_uint<T>(s, t.minor); return;
            case CBOR_NINT: val = -1 -get_uint<T>(s, t.minor); return;
            default: throw std::runtime_error("unexpected type");
        }
    }

    template<class T>
    typename std::enable_if<
        std::is_trivial<T>::value, void
    >::type
    read(imemstream& s, std::vector<T>& data) {
        size_t len = get_uint(s, ensure_type(s, CBOR_BINARY));
        data.resize(len / sizeof(T));
        s.read(&data[0], len);
    }

    inline void
    read(imemstream& s, binary_ref& data) {
        size_t len = get_uint(s, ensure_type(s, CBOR_BINARY));
        auto tmp = s.substring(len);
        data = boost::asio::const_buffer(tmp.data(), tmp.size());
    }

    inline void read(imemstream& s, std::string& str) {
        size_t len = get_uint(s, ensure_type(s, CBOR_STR));
        str.resize(len);
        s.read(&str[0], len);
    }

    inline void read(imemstream& s, boost::string_ref& str) {
        size_t len = get_uint(s, ensure_type(s, CBOR_STR));
        str = s.substring(len);
    }

    inline void read(imemstream& s, float& v) {
        if (ensure_type(s, CBOR_X) == CBOR_FLOAT) {
            union {
                float f;
                uint32_t u;
            } un;
            s.read(&un.u, sizeof(un.u));
            un.u = be2h(un.u);
            v = un.f;
            return;
        }
        throw std::runtime_error("unexpected special");
    }
    inline void read(imemstream& s, double& v) {
        if (ensure_type(s, CBOR_X) == CBOR_DOUBLE) {
            union {
                double d;
                uint64_t u;
            } un;
            s.read(&un.u, sizeof(un.u));
            un.u = be2h(un.u);
            v = un.d;
            return;
        }
        throw std::runtime_error("unexpected special");
    }
    inline void read(imemstream& s, bool& v) {
        switch(ensure_type(s, CBOR_X)) {
            case CBOR_FALSE: v = false; return;
            case CBOR_TRUE:  v = true; return;
            default: throw std::runtime_error("unexpected special");
        }
    }

    template<class T>
    void read(imemstream& s, std::list<T>& l)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_LIST));
        for (size_t i = 0; i < mt; i++) {
            l.emplace_back();
            read(s, l.back());
        }
    }

    template<class K, class V>
    void read(imemstream& s, std::map<K, V>& map)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_MAP));
        for (size_t i = 0; i < mt; i++) {
            K key;
            V value;
            read(s, key);
            read(s, value);
            map.insert(std::make_pair(key, value));
        }
    }

    inline bool read_tag(imemstream& s, uint64_t& tag) {
        const auto t = get_type(s, false);
        if (t.major == CBOR_TAG) {
            tag = get_uint(s, t.minor);
            return true;
        }
        s.unget();
        return false;
    }
} // namespace