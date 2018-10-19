#pragma once

#include <boost/utility/string_ref.hpp>
#include <stdexcept>
#include <string>

/*
    dirty and incomplete parser for protobuf

    manual:
    https://developers.google.com/protocol-buffers/docs/encoding

    reason to make one:
    Benchmark           Time           CPU Iterations
    --------------------------------------------------
    BM_Google         109 ns        109 ns    5359322
    BM_Custom          28 ns         28 ns   25246605
*/

namespace Protobuf
{
    using Buffer = boost::string_ref;
    struct EndOfBuffer : std::runtime_error { EndOfBuffer() : std::runtime_error("End of buffer") {} };
    struct BadTag      : std::runtime_error { BadTag()      : std::runtime_error("Bad tag") {} };
    struct BadInput    : std::runtime_error { BadInput()    : std::runtime_error("Bad input") {} };

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

    public:

        Walker(const Buffer& aBuffer)
        : m_Buffer(aBuffer)
        { }

        FieldInfo readTag()
        {
            return FieldInfo(readByte());
        }

        // ZigZag not supported (sint32/64 in schema)
        // fixed width not supported (fixed64, sfixed64, double, fixed32, sfixed32, float)
        uint64_t readVarUInt() { return readVarInt<uint64_t>(); }
        int64_t  readVarInt()  { return readVarInt<int64_t>(); }

        std::string readString()
        {
            const auto sSize = readVarUInt();
            if (sSize > m_Buffer.size())
                throw BadInput();

            std::string sResult(m_Buffer.data(), sSize);
            m_Buffer.remove_prefix(sSize);
            return sResult;
        }

        void skip(const FieldInfo& aField)
        {
            int sSize = aField.decodeSize();
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
            sSize = readVarUInt();
            if (sSize > m_Buffer.size())
                throw BadInput();

            m_Buffer.remove_prefix(sSize);
        }

        template<class T>
        void parse(T aCallback)
        {
            // user must consume or skip data
            while (!m_Buffer.empty())
                aCallback(readTag(), this);
        }

        bool empty() const
        {
            return m_Buffer.empty();
        }
    };
}
