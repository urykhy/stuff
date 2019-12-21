#pragma once
#include <container/Stream.hpp>
#include <list>

namespace MsgPack
{
    using binary = Container::binary;
    using omemstream = Container::omemstream;
    using imemstream = Container::imemstream;

    struct BadFormat : std::runtime_error { BadFormat() : std::runtime_error("Bad msgpack format") {} };

    enum
    {
        FIXMAP   = 0x80,
        FIXARRAY = 0x90,
        FIXSTR   = 0xa0,
        NIL      = 0xc0,

        PACK_UINT8  = 0xcc,
        PACK_UINT16 = 0xcd,
        PACK_UINT32 = 0xce,
        PACK_UINT64 = 0xcf,

        PACK_MAP16  = 0xde,
        PACK_MAP32  = 0xdf,

        PACK_ARRAY16  = 0xdc,
        PACK_ARRAY32  = 0xdd,

        PACK_STR8   = 0xd9,
        PACK_STR16  = 0xda,
        PACK_STR32  = 0xdb,
    }; // https://github.com/msgpack/msgpack/blob/master/spec.md

    inline void mp_store_u8(omemstream& aStream, uint8_t aValue)   { aStream.put(aValue); }
    inline void mp_store_u16(omemstream& aStream, uint16_t aValue) { aValue = htobe16(aValue); aStream.write(&aValue, sizeof(aValue)); }
    inline void mp_store_u32(omemstream& aStream, uint32_t aValue) { aValue = htobe32(aValue); aStream.write(&aValue, sizeof(aValue)); }
    inline void mp_store_u64(omemstream& aStream, uint64_t aValue) { aValue = htobe64(aValue); aStream.write(&aValue, sizeof(aValue)); }

    inline uint8_t  mp_load_u8(imemstream& aStream)  { uint8_t sValue;  aStream.read(&sValue, sizeof(sValue)); return sValue; }
    inline uint64_t mp_load_u16(imemstream& aStream) { uint16_t sValue; aStream.read(&sValue, sizeof(sValue)); return be16toh(sValue); }
    inline uint64_t mp_load_u32(imemstream& aStream) { uint32_t sValue; aStream.read(&sValue, sizeof(sValue)); return be32toh(sValue); }
    inline uint64_t mp_load_u64(imemstream& aStream) { uint64_t sValue; aStream.read(&sValue, sizeof(sValue)); return be64toh(sValue); }

    inline void write_uint(omemstream& aStream, uint64_t aValue)
    {
        if (aValue < FIXMAP) {
            mp_store_u8(aStream, aValue);
        } else if (aValue <= UINT8_MAX) {
            mp_store_u8(aStream, PACK_UINT8);
            mp_store_u8(aStream, aValue);
        } else if (aValue <= UINT16_MAX) {
            mp_store_u8(aStream, PACK_UINT16);
            mp_store_u16(aStream, aValue);
        } else if (aValue <= UINT32_MAX) {
            mp_store_u8(aStream, PACK_UINT32);
            mp_store_u32(aStream, aValue);
        } else {
            mp_store_u8(aStream, PACK_UINT64);
            mp_store_u64(aStream, aValue);
        }
    }

    template<class T>
    void read_uint(imemstream& aStream, T& aValue)
    {
        uint8_t sCode = mp_load_u8(aStream);

        switch (sCode) {
        case PACK_UINT8:  { uint8_t  res = 0; aStream.read(&res, sizeof(res)); aValue = res; break; }
        case PACK_UINT16: { uint16_t res = 0; aStream.read(&res, sizeof(res)); aValue = be16toh(res); break; }
        case PACK_UINT32: { uint32_t res = 0; aStream.read(&res, sizeof(res)); aValue = be32toh(res); break; }
        case PACK_UINT64: { uint64_t res = 0; aStream.read(&res, sizeof(res)); aValue = be64toh(res); break; }
        default:
            if (sCode >= FIXMAP) throw BadFormat();
            aValue = (uint8_t) sCode;
        }
    }

    inline void write_map_size(omemstream& aStream, uint32_t aSize)
    {
        if (aSize <= 0x0f) {
            mp_store_u8(aStream, FIXMAP | aSize);
        } else if (aSize <= UINT16_MAX) {
            mp_store_u8(aStream, PACK_MAP16);
            mp_store_u16(aStream, aSize);
        } else {
            mp_store_u8(aStream, PACK_MAP32);
            mp_store_u32(aStream, aSize);
        }
    }

    inline uint32_t read_map_size(imemstream& aStream)
    {
        uint8_t sCode = mp_load_u8(aStream);
        switch (sCode) {
        case PACK_MAP16: return mp_load_u16(aStream);
        case PACK_MAP32: return mp_load_u32(aStream);
        default:
            if (sCode < FIXMAP || sCode >= FIXARRAY) throw BadFormat();
            return sCode & 0x0f;
        }
    }

    inline void write_array_size(omemstream& aStream, uint32_t aSize)
    {
        if (aSize <= 0x0f) {
            mp_store_u8(aStream, FIXARRAY | aSize);
        } else if (aSize <= UINT16_MAX) {
            mp_store_u8(aStream, PACK_ARRAY16);
            mp_store_u16(aStream, aSize);
        } else {
            mp_store_u8(aStream, PACK_ARRAY32);
            mp_store_u32(aStream, aSize);
        }
    }

    inline uint32_t read_array_size(imemstream& aStream)
    {
        uint8_t sCode = mp_load_u8(aStream);

        switch (sCode) {
        case PACK_ARRAY16: return mp_load_u16(aStream);
        case PACK_ARRAY32: return mp_load_u32(aStream);
        default:
            if (sCode < FIXARRAY || sCode >= FIXSTR) throw BadFormat();
            return sCode & 0x0f;
        }
    }

    inline void write_string(omemstream& aStream, const std::string& aString)
    {
        auto aStringSize = aString.size();
        if (aStringSize <= 0x1f) {
            mp_store_u8(aStream, FIXSTR | (uint8_t) aStringSize);
        } else if (aStringSize <= UINT8_MAX) {
            mp_store_u8(aStream, PACK_STR8);
            mp_store_u8(aStream, aStringSize);
        } else if (aStringSize <= UINT16_MAX) {
            mp_store_u8(aStream, PACK_STR16);
            mp_store_u16(aStream, aStringSize);
        } else {
            mp_store_u8(aStream, PACK_STR32);
            mp_store_u32(aStream, aStringSize);
        }
        aStream.write(aString.data(), aStringSize);
    }

    inline void read_string(imemstream& aStream, std::string& aString)
    {
        uint32_t sStringSize = 0;
        uint8_t sCode = mp_load_u8(aStream);
        switch (sCode) {
        case PACK_STR8:  sStringSize = mp_load_u8(aStream);  break;
        case PACK_STR16: sStringSize = mp_load_u16(aStream); break;
        case PACK_STR32: sStringSize = mp_load_u32(aStream); break;
        default:
            if (sCode < FIXSTR || sCode >= NIL) throw BadFormat();
            sStringSize = sCode & 0x1f;
        }
        const auto sString = aStream.substring(sStringSize);
        aString.assign(sString.data(), sString.size());
    }

    template<class T>
    inline void write_array(omemstream& aStream, const std::list<T>& aList)
    {
        write_array_size(aStream, aList.size());
        for (const auto& x : aList)
            write(aStream, x);
    }
}
