#pragma once
#include <endian.h>
#include <cstdint>
#include <sstream>
#include <string>
#include <boost/utility/string_ref.hpp>
#include <boost/asio/buffer.hpp>
#include <vector>
#include <list>
#include <map>
#include <string.h>
#include <cassert>

namespace cbor {

    typedef boost::asio::const_buffer binary_ref;
    typedef std::vector<uint8_t> binary;

    // custom `streams` to support string_ref
    // and operate faster than stl streams

    enum {
        CBOR_UINT   = 0,
        CBOR_NINT   = 1,
        CBOR_BINARY = 2,
        CBOR_STR    = 3,
        CBOR_LIST   = 4,
        CBOR_MAP    = 5,
        CBOR_TAG    = 6,
        CBOR_X      = 7,

        CBOR_FALSE      = 20,
        CBOR_TRUE       = 21,
        CBOR_8          = 24,
        CBOR_16         = 25,
        CBOR_32         = 26,
        CBOR_64         = 27,
        CBOR_FLOAT      = 26,
        CBOR_DOUBLE     = 27,
    };

    class imemstream
    {
        const boost::string_ref src;
        size_t offset = 0;
    public:

        imemstream(const binary_ref data)
        : src(boost::asio::buffer_cast<const char*>(data), boost::asio::buffer_size(data))
        {
            ;;
        }
        imemstream(const binary& s)
        : src((const char*)&s[0], s.size())
        {
            ;;
        }

        boost::string_ref substring(size_t len) {
            if (offset + len > src.size()) {
                throw std::runtime_error("no more data");
            }
            boost::string_ref s = src.substr(offset, len);
            offset += len;
            return s;
        }
        void read(void* dest, size_t len) {
            if (offset + len > src.size()) {
                throw std::runtime_error("no more data");
            }
            memcpy(dest, src.data() + offset, len);
            offset += len;
        }
        void unget() {
            assert (offset > 0);
            offset --;
        }
        void skip(size_t len) {
            if (offset + len > src.size()) {
                throw std::runtime_error("no more data");
            }
            offset += len;
        }
        bool empty() const {
            return offset == src.size();
        }
    };

    class omemstream
    {
        binary& dest;
    public:
        omemstream(binary& b) : dest(b) {}
        void put(char c) {
            dest.push_back(c);
        }
        void write(const void* src, size_t len) {
            dest.insert(dest.end(), (const uint8_t*)src, (const uint8_t*)src +len);
        }
    };

    inline std::string to_string(const binary_ref data) {
        return std::string(boost::asio::buffer_cast<const char*>(data), boost::asio::buffer_size(data));
    }

    inline binary to_binary(const binary_ref data) {
        binary tmp;
        tmp.assign(boost::asio::buffer_cast<const uint8_t*>(data),
                   boost::asio::buffer_cast<const uint8_t*>(data) + boost::asio::buffer_size(data));
        return tmp;
    }

    inline auto to_binary_ref(const binary& data) {
        return binary_ref(&data[0], data.size());
    }
}