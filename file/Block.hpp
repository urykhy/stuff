#pragma once

#include <string_view>
#include <filesystem>

#include "File.hpp"
#include <ssl/Digest.hpp>

namespace File::Block
{
    struct Header
    {
        uint32_t magic;
        uint32_t size;
        uint64_t hash;
        uint8_t  type;

        enum { MAGIC = 0x02030405 };
    } __attribute__((packed));

    namespace io = boost::iostreams;

    inline uint64_t hash(const Header& aHeader, std::string_view aData)
    {
        return SSLxx::DigestHash(EVP_md5()
                               , aData
                               , std::string_view((const char*)&aHeader.size, sizeof(aHeader.size))
                               , std::string_view((const char*)&aHeader.type, sizeof(aHeader.type)));
    }

    // if chunk written is more than `aBufferSize-sizeof(Header)` - it can't be read with default settings by Reader
    class Writer
    {
        const std::string     m_Name;
        const std::string     m_TmpName;
        std::ofstream         m_File;
        io::filtering_ostream m_Stream;

    public:
        Writer(const std::string& aName)
        : m_Name(aName)
        , m_TmpName(std::filesystem::path(get_basename(aName)) /= (".tmp." + get_filename(aName)))
        {
            m_File.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            m_File.open(m_TmpName, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
            add_compressor(m_Stream, get_format(get_extension(aName)));
            m_Stream.push(m_File);
            m_Stream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        }

        void operator()(uint8_t aType, std::string_view aData)
        {
            Header sHeader{Header::MAGIC, static_cast<uint32_t>(aData.size()), 0, aType};
            sHeader.hash = hash(sHeader, aData);
            m_Stream.write(reinterpret_cast<const char*>(&sHeader), sizeof(sHeader));
            m_Stream.write(aData.data(), aData.size());
        }

        void close()
        {
            m_Stream.flush();
            m_Stream.reset();
            std::filesystem::rename(m_TmpName, m_Name);
        }
    };

    template<class T>
    void Reader(const std::string& aName, T aHandler, uint64_t aBufferSize = 1024 * 1024)
    {
        by_chunk(aName, [aHandler = std::move(aHandler)](std::string_view aData)
        {
            uint64_t sUsed = 0;
            while (aData.size() > sizeof(Header))
            {
                const Header* sHeader = reinterpret_cast<const Header*>(aData.data());
                if (sHeader->magic != Header::MAGIC)
                    throw std::runtime_error("File::Part::Reader: bad magic");
                if (aData.size() < sHeader->size)
                    break;
                std::string_view sData(aData.data() + sizeof(Header), sHeader->size);
                uint64_t sHash = hash(*sHeader, sData);
                if (sHeader->hash != sHash)
                    throw std::runtime_error("File::part::Reader: bad hash");
                aHandler(sHeader->type, sData);
                sUsed += (sizeof(Header) + sHeader->size);
                aData.remove_prefix(sizeof(Header) + sHeader->size);
            }
            return sUsed;
        }, aBufferSize);
    }

} // namespace File
