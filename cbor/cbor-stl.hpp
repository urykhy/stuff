#pragma once
#include "cbor-basic.hpp"

namespace cbor {

    // string
    inline void read(istream& s, std::string& str)
    {
        size_t len = get_uint(s, ensure_type(s, CBOR_STR));
        str.resize(len);
        s.read(&str[0], len);
    }

    inline void read(File::IMemReader& s, std::string_view& str)
    {
        size_t len = get_uint(s, ensure_type(s, CBOR_STR));
        str        = s.substring(len);
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

    // list
    template <class T>
    void read(istream& s, std::list<T>& l)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_LIST));
        for (size_t i = 0; i < mt; i++) {
            l.emplace_back();
            read(s, l.back());
        }
    }

    template <class T>
    void write(ostream& out, const std::list<T>& l)
    {
        write_type_value(out, CBOR_LIST, l.size());
        for (const auto& x : l) {
            write(out, x);
        }
    }

    // vector
    template <class T>
    typename std::enable_if<std::is_trivial<T>::value, void>::type
    read(istream& s, std::vector<T>& data)
    {
        size_t len = get_uint(s, ensure_type(s, CBOR_BINARY));
        data.resize(len / sizeof(T));
        s.read(&data[0], len);
    }

    template <class T>
    typename std::enable_if<std::is_trivial<T>::value, void>::type
    write(ostream& out, const std::vector<T>& data)
    {
        write_type_value(out, CBOR_BINARY, data.size() * sizeof(T));
        out.write(data.data(), data.size() * sizeof(T));
    }

    template <class T>
    void read(istream& s, std::vector<T>& l)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_LIST));
        l.reserve(mt);
        for (size_t i = 0; i < mt; i++) {
            l.emplace_back();
            read(s, l.back());
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

    // map
    template <class K, class V>
    void read(istream& s, std::map<K, V>& map)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_MAP));
        for (size_t i = 0; i < mt; i++) {
            K key;
            V value;
            read(s, key);
            read(s, value);
            map.emplace(std::move(key), std::move(value));
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

    // unordered map
    template <class K, class V>
    void read(istream& s, std::unordered_map<K, V>& map)
    {
        size_t mt = get_uint(s, ensure_type(s, CBOR_MAP));
        for (size_t i = 0; i < mt; i++) {
            K key;
            V value;
            read(s, key);
            read(s, value);
            map.emplace(std::move(key), std::move(value));
        }
    }

    template <class K, class V>
    void write(ostream& out, const std::unordered_map<K, V>& map)
    {
        write_type_value(out, CBOR_MAP, map.size());
        for (const auto& x : map) {
            write(out, x.first);
            write(out, x.second);
        }
    }
} // namespace cbor
