#pragma once

#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

/*
    dirty and incomplete parser for protobuf

    manual:
    https://developers.google.com/protocol-buffers/docs/encoding

    reason to make one:
    Benchmark                Time           CPU Iterations
    -------------------------------------------------------
    BM_GoogleParse             310 ns        310 ns    2261366
    BM_GoogleParseReuse        176 ns        176 ns    3999000
    BM_CustomParse              52 ns         52 ns   13361702
    BM_GoogleSerialize         130 ns        130 ns    5400724
    BM_CustomSerialize          71 ns         71 ns    9866489

*/

namespace Protobuf {
    struct EndOfBuffer : std::runtime_error
    {
        EndOfBuffer()
        : std::runtime_error("End of buffer")
        {}
    };
    struct BadTag : std::runtime_error
    {
        BadTag()
        : std::runtime_error("Bad tag")
        {}
    };
    struct BadInput : std::runtime_error
    {
        BadInput()
        : std::runtime_error("Bad input")
        {}
    };

    enum Action
    {
        ACT_USED,
        ACT_SKIP,
        ACT_BREAK
    };

    struct FieldInfo
    {
        uint8_t  tag = -1;
        uint64_t id  = -1;

        enum
        {
            TAG_VARIANT = 0,
            TAG_FIXED64 = 1,
            TAG_FIXED32 = 5,
            TAG_LENGTH  = 2
        };

        FieldInfo(uint64_t aTag)
        : tag((uint8_t)(aTag & 0x07))
        , id(aTag >> 3)
        {}

        FieldInfo(uint8_t aTag, uint64_t aId)
        : tag(aTag)
        , id(aId)
        {}

        int decodeSize() const
        {
            if (tag == TAG_VARIANT)
                return 1;
            if (tag == TAG_FIXED32)
                return 4;
            if (tag == TAG_FIXED64)
                return 8;
            if (tag == TAG_LENGTH)
                return 0;
            throw BadTag();
        }
    };

    enum IntType
    {
        VARIANT = 0, // standard variable-length encoding
        FIXED,       // fixed/sfixed
        ZIGZAG,      // sint
    };

    // allow to decode some fields from protobuf
    // and skip others
    class Reader
    {
        std::string_view m_Buffer;

        uint8_t readByte()
        {
            if (m_Buffer.empty())
                throw EndOfBuffer();

            uint8_t sByte = m_Buffer.front();
            m_Buffer.remove_prefix(1);
            return sByte;
        }

#ifdef BOOST_TEST_MODULE // open for tests only
    public:
#endif
        FieldInfo readTag()
        {
            return FieldInfo(readVarInt<uint64_t>());
        }

        template <class T>
        T readVarInt()
        {
            T       sValue = 0;
            int     sShift = 0;
            uint8_t sByte  = 0;

            do {
                if (sShift > 63)
                    throw BadInput();
                sByte = readByte();
                sValue |= (((T)sByte & 0x7F) << sShift);
                sShift += 7;
            } while (sByte >= 0x80); // Each byte in a varint, except the last byte, has the most significant bit (msb) set

            return sValue;
        }

        void skip(const FieldInfo& aField)
        {
            size_t sSize = aField.decodeSize();
            if (sSize == 4 || sSize == 8) {
                if (sSize > m_Buffer.size())
                    throw BadInput();
                else
                    m_Buffer.remove_prefix(sSize);
                return;
            }

            // VarInt
            if (sSize == 1) {
                readVarInt<size_t>();
                return;
            }

            // Length-delimited
            sSize = readVarInt<size_t>();
            if (sSize > m_Buffer.size())
                throw BadInput();

            m_Buffer.remove_prefix(sSize);
        }

        // from wire_format_lite.h, unsigned to signed
        template <class T>
        typename std::make_signed<T>::type ZigZagDecode(T n) { return (n >> 1) ^ -static_cast<typename std::make_signed<T>::type>(n & 1); }

        template <class T>
        void readFixed(T& aDest)
        {
            if (sizeof(T) > m_Buffer.size())
                throw BadInput();
            memcpy(&aDest, m_Buffer.data(), sizeof(T));
            m_Buffer.remove_prefix(sizeof(T));
        }

    public:
        Reader(std::string_view aBuffer)
        : m_Buffer(aBuffer)
        {}

        // get destination in argument
        template <class T>
        typename std::enable_if<
            std::is_floating_point<T>::value, void>::type
        read(T& aDest)
        {
            readFixed(aDest);
        }

        template <class T>
        typename std::enable_if<
            std::is_integral<T>::value, void>::type
        read(T& aDest, IntType mode = VARIANT)
        {
            switch (mode) {
            case VARIANT: aDest = readVarInt<T>(); break;
            case FIXED: readFixed(aDest); break;
            case ZIGZAG: aDest = ZigZagDecode(readVarInt<typename std::make_unsigned<T>::type>()); break;
            }
        }

        template <class T>
        typename std::enable_if<
            std::is_class<T>::value, void>::type
        read(T& aDest)
        {
            const auto sSize = readVarInt<size_t>();
            if (sSize > m_Buffer.size())
                throw BadInput();

            aDest.assign(m_Buffer.data(), sSize);
            m_Buffer.remove_prefix(sSize);
        }

        void read(std::string_view& aDest)
        {
            const auto sSize = readVarInt<size_t>();
            if (sSize > m_Buffer.size())
                throw BadInput();

            aDest = std::string_view(m_Buffer.data(), sSize);
            m_Buffer.remove_prefix(sSize);
        }

        template <class T>
        void parse(T aCallback)
        {
            while (!m_Buffer.empty()) {
                const auto sField = readTag();
                switch (aCallback(sField, this)) {
                case ACT_USED: break;
                case ACT_SKIP: skip(sField); break;
                case ACT_BREAK: return;
                }
            }
        }

        bool empty() const
        {
            return m_Buffer.empty();
        }
    };

    class Writer
    {
        std::string& m_Buffer;

        template <class T>
        void writeVarInt(T aVar)
        {
            do {
                uint8_t sByte = aVar & 0x7F;
                aVar >>= 7;
                if (aVar != 0)
                    sByte |= 0x80;
                m_Buffer.push_back(sByte);
            } while (aVar > 0);
        }

        void writeTag(FieldInfo aInfo)
        {
            writeVarInt(aInfo.tag | (aInfo.id << 3));
        }

    public:
        Writer(std::string& aStr)
        : m_Buffer(aStr)
        {}

        template <class T>
        typename std::enable_if<
            std::is_integral<T>::value, void>::type
        write(uint64_t aId, T aVal, IntType mode = VARIANT)
        {
            switch (mode) {
            case VARIANT:
                writeTag({Protobuf::FieldInfo::TAG_VARIANT, aId});
                writeVarInt(aVal);
                break;
            case FIXED: throw std::invalid_argument("fixed not implemented");
            case ZIGZAG: throw std::invalid_argument("zigzag not implemented");
            }
        }

        void write(uint64_t aId, std::string_view aStr)
        {
            writeTag({Protobuf::FieldInfo::TAG_LENGTH, aId});
            writeVarInt(aStr.size());
            m_Buffer.append(aStr);
        }
    };
} // namespace Protobuf
