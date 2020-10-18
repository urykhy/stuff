#pragma once

#include <stdint.h>

#include <ssl/Digest.hpp>

#include "File.hpp"

namespace File::Block {
    struct Header
    {
        uint32_t magic = MAGIC;
        uint32_t size  = 0;
        uint64_t hash  = 0;
        uint8_t  type  = 0;

        enum
        {
            MAGIC = 0x02030405
        };
    } __attribute__((packed));

    inline uint64_t hash(const Header& aHeader, std::string_view aData)
    {
        return SSLxx::DigestHash(EVP_md5(), aData, aHeader.size, aHeader.type);
    }

    // T must accept Header.type, std::string&
    template <class T>
    inline void read(const std::string& aName, T&& aHandler)
    {
        ::File::read(aName, [aHandler](IReader* aReader) mutable {
            Header sHeader;
            while (not aReader->eof()) {
                size_t sSize = aReader->read((char*)&sHeader, sizeof(sHeader));
                if (sSize == 0)
                    break;
                if (sSize != sizeof(sHeader))
                    throw std::runtime_error("File::Block: header too short");
                if (sHeader.magic != Header::MAGIC)
                    throw std::runtime_error("File::Block: bad magic");
                std::string sTmp(sHeader.size, ' ');
                sSize = aReader->read(&sTmp[0], sTmp.size());
                if (sSize != sTmp.size())
                    throw std::runtime_error("File::Block: data too short");
                if (sHeader.hash != hash(sHeader, sTmp))
                    throw std::runtime_error("File::Block: bad hash");
                aHandler(sHeader.type, sTmp);
            }
        });
    }

    // T must call Api::write
    template <class T>
    inline void write(const std::string& aName, T&& aHandler)
    {
        struct Api
        {
            IWriter* m_Parent = nullptr;
            Api(IWriter* aWriter)
            : m_Parent(aWriter)
            {}
            void write(uint8_t aType, std::string_view aData)
            {
                Header sHeader;
                sHeader.type = aType;
                sHeader.size = aData.size();
                sHeader.hash = hash(sHeader, aData);
                m_Parent->write((const char*)&sHeader, sizeof(sHeader));
                m_Parent->write(&aData[0], aData.size());
            }
            ~Api() {}
        };
        ::File::write(aName, [aHandler](IWriter* aWriter) mutable {
            Api sApi(aWriter);
            aHandler(&sApi);
        });
    }

} // namespace File::Block