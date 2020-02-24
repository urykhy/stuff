#pragma once

#include <boost/utility/string_ref.hpp>
#include <cstring>
#include <stdexcept>
#include <string>

/*
    dirty and incomplete parser for protobuf

    manual:
    https://developers.google.com/protocol-buffers/docs/encoding

    reason to make one:
    Benchmark                Time           CPU Iterations
    -------------------------------------------------------
    BM_Google              277 ns        277 ns    2525099
    BM_Google_reuse        148 ns        148 ns    4738163
    BM_Custom               65 ns         64 ns   10873117

*/

namespace Protobuf
{
    using Buffer = boost::string_ref;
    struct EndOfBuffer : std::runtime_error { EndOfBuffer() : std::runtime_error("End of buffer") {} };
    struct BadTag      : std::runtime_error { BadTag()      : std::runtime_error("Bad tag") {} };
    struct BadInput    : std::runtime_error { BadInput()    : std::runtime_error("Bad input") {} };

    enum Action {
        ACT_USED
      , ACT_SKIP
      , ACT_BREAK
    };

    struct FieldInfo
    {
        int8_t tag = -1;
        int8_t id = -1;

        enum
        {
            TAG_VARIANT = 0
          , TAG_FIXED64 = 1
          , TAG_FIXED32 = 5
          , TAG_LENGTH  = 2
        };

        FieldInfo(uint8_t aTag)
        : tag(aTag & 0x07)
        , id(aTag >> 3)
        { }

        int decodeSize() const
        {
            if (tag == TAG_VARIANT) return 1;
            if (tag == TAG_FIXED32) return 4;
            if (tag == TAG_FIXED64) return 8;
            if (tag == TAG_LENGTH)  return 0;
            throw BadTag();
        }
    };

    // allow to decode some fields from protobuf
    // and skip others
    class Walker
    {
        Buffer m_Buffer;

        uint8_t readByte()
        {
            if (m_Buffer.empty())
                throw EndOfBuffer();

            uint8_t sByte = m_Buffer.front();
            m_Buffer.remove_prefix(1);
            return sByte;
        }

        void skipVarInt()
        {
            uint8_t sByte = 0;
            do {
                sByte = readByte();
            } while (sByte > 0x80); // Each byte in a varint, except the last byte, has the most significant bit (msb) set
        }

#ifdef BOOST_TEST_DYN_LINK  // open for tests only
    public:
#endif
        FieldInfo readTag()
        {
            return FieldInfo(readByte());
        }

        template<class T>
        T readVarInt()
        {
            T sValue = 0;
            int sShift = 0;
            uint8_t sByte = 0;

            do {
                if (sShift > 63)
                    throw BadInput();
                sByte = readByte();
                sValue |= (((T)sByte & 0x7F) << sShift);
                sShift += 7;
            } while (sByte > 0x80); // Each byte in a varint, except the last byte, has the most significant bit (msb) set

            return sValue;
        }

        std::string readString()
        {
            const auto sSize = readVarInt<size_t>();
            if (sSize > m_Buffer.size())
                throw BadInput();

            std::string sResult(m_Buffer.data(), sSize);
            m_Buffer.remove_prefix(sSize);
            return sResult;
        }

        void skip(const FieldInfo& aField)
        {
            size_t sSize = aField.decodeSize();
            if (sSize == 4 || sSize == 8)
            {
                m_Buffer.remove_prefix(sSize);
                return;
            }

            // VarInt
            if (sSize == 1)
            {
                skipVarInt();
                return;
            }

            // Length-delimited
            sSize = readVarInt<size_t>();
            if (sSize > m_Buffer.size())
                throw BadInput();

            m_Buffer.remove_prefix(sSize);
        }

        // from wire_format_lite.h, unsigned to signed
        template<class T> typename std::make_signed<T>::type ZigZagDecode(T n) { return (n >> 1) ^ -static_cast<typename std::make_signed<T>::type>(n & 1); }

        template<class T>
        void readFixed(T& aDest)
        {
            if (sizeof(T) > m_Buffer.size())
                throw BadInput();
            memcpy(&aDest, m_Buffer.data(), sizeof(T));
            m_Buffer.remove_prefix(sizeof(T));
        }

    public:

        Walker(const Buffer& aBuffer)
        : m_Buffer(aBuffer)
        { }

        enum IntType {
            VARIANT = 0, // standard variable-length encoding
            FIXED,      // fixed/sfixed
            ZIGZAG,     // sint
        };

        // get destination in argument
        template<class T>
        typename std::enable_if<
            std::is_floating_point<T>::value, void
        >::type
        read(T& aDest) {
            readFixed(aDest);
        }

        template<class T>
        typename std::enable_if<
            std::is_integral<T>::value, void
        >::type
        read(T& aDest, IntType mode = VARIANT) {
            switch (mode)
            {
            case VARIANT: aDest = readVarInt<T>(); break;
            case FIXED  : readFixed(aDest); break;
            case ZIGZAG : aDest = readVarInt<T>(); aDest = ZigZagDecode(aDest); break;
            }
        }

        template<class T>
        typename std::enable_if<
            std::is_class<T>::value, void
        >::type
        read(T& aDest)
        {
            const auto sSize = readVarInt<size_t>();
            if (sSize > m_Buffer.size())
                throw BadInput();

            aDest = T(m_Buffer.data(), sSize);
            m_Buffer.remove_prefix(sSize);
        }

        template<class T>
        void parse(T aCallback)
        {
            while (!m_Buffer.empty())
            {
                const auto sField = readTag();
                switch (aCallback(sField, this))
                {
                    case ACT_USED:  break;
                    case ACT_SKIP:  skip(sField); break;
                    case ACT_BREAK: return;
                }
            }
        }

        bool empty() const
        {
            return m_Buffer.empty();
        }
    };
}
