#pragma once
#include "cbor-internals.hpp"

namespace cbor {

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
    };

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

    template <class F = std::function<void(uint64_t)>>
    inline TypeInfo get_type(
        istream& s, F& onTag = [](auto) {})
    {
        TypeInfo info{0};
        while (1) {
            s.read(&info, sizeof(info));
            if (info.major == CBOR_TAG) {
                onTag(get_uint(s, info.minor));
                continue;
            }
            break;
        }
        return info;
    }

    template <class F = std::function<void(uint64_t)>>
    inline uint8_t ensure_type(
        istream& s, uint8_t needType, F&& onTag = [](uint64_t) {})
    {
        TypeInfo t = get_type(s, onTag);
        if (t.major != needType)
            throw std::invalid_argument("unexpected major type: " + std::to_string(t.major));
        return t.minor;
    }

    // only positive int
    template <class T, class F = std::function<void(uint64_t)>>
    typename std::enable_if<
        std::is_unsigned<T>::value, void>::type
    read(
        istream& s, T& val, F&& onTag = [](uint64_t) {})
    {
        val = get_uint<T>(s, ensure_type(s, CBOR_UINT, onTag));
    }

    // positive + negative integer
    template <class T, class F = std::function<void(uint64_t)>>
    typename std::enable_if<
        std::is_signed<T>::value, void>::type
    read(
        istream& s, T& val, F&& onTag = [](uint64_t) {})
    {
        const auto t = get_type(s, onTag);
        switch (t.major) {
        case CBOR_UINT: val = get_uint<T>(s, t.minor); return;
        case CBOR_NINT: val = -1 - get_uint<T>(s, t.minor); return;
        default: throw std::invalid_argument("unexpected major type: " + std::to_string(t.major));
        }
    }

    template <class T, class F = std::function<void(uint64_t)>>
    typename std::enable_if<
        std::is_trivial<T>::value, void>::type
    read(
        istream& s, std::vector<T>& data, F&& onTag = [](uint64_t) {})
    {
        size_t len = get_uint(s, ensure_type(s, CBOR_BINARY, onTag));
        data.resize(len / sizeof(T));
        s.read(&data[0], len);
    }

    template <class F = std::function<void(uint64_t)>>
    inline void read(
        istream& s, std::string& str, F&& onTag = [](uint64_t) {})
    {
        size_t len = get_uint(s, ensure_type(s, CBOR_STR, onTag));
        str.resize(len);
        s.read(&str[0], len);
    }

    template <class F = std::function<void(uint64_t)>>
    inline void read(
        File::IMemReader& s, std::string_view& str, F&& onTag = [](uint64_t) {})
    {
        size_t len = get_uint(s, ensure_type(s, CBOR_STR, onTag));
        str        = s.substring(len);
    }

    template <class F = std::function<void(uint64_t)>>
    inline void read(
        istream& s, float& v, F&& onTag = [](uint64_t) {})
    {
        auto minorType = ensure_type(s, CBOR_X, onTag);
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

    template <class F = std::function<void(uint64_t)>>
    inline void read(
        istream& s, double& v, F&& onTag = [](uint64_t) {})
    {
        auto minorType = ensure_type(s, CBOR_X, onTag);
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

    template <class F = std::function<void(uint64_t)>>
    inline void read(
        istream& s, bool& v, F&& onTag = [](uint64_t) {})
    {
        auto minorType = ensure_type(s, CBOR_X, onTag);
        switch (minorType) {
        case CBOR_FALSE: v = false; return;
        case CBOR_TRUE: v = true; return;
        default: throw std::invalid_argument("unexpected special: " + std::to_string(minorType));
        }
    }

    template <class T, class F = std::function<void(uint64_t)>>
    void read(
        istream& s, std::list<T>& l, F&& onTag = [](uint64_t) {})
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_LIST, onTag));
        for (size_t i = 0; i < mt; i++) {
            l.emplace_back();
            read(s, l.back());
        }
    }

    template <class K, class V, class F = std::function<void(uint64_t)>>
    void read(
        istream& s, std::map<K, V>& map, F&& onTag = [](uint64_t) {})
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_MAP, onTag));
        for (size_t i = 0; i < mt; i++) {
            K key;
            V value;
            read(s, key);
            read(s, value);
            map.emplace(std::move(key), std::move(value));
        }
    }
} // namespace cbor