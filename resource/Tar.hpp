#pragma once

#include <algorithm>
#include <map>
#include <string_view>
#include <string.h>
#include <string>
#include "parser/Atoi.hpp"

namespace resource
{
    // no long names.

    struct Tar
    {
        struct header
        {
            char name[100];
            char mode[8];
            char owner[8];
            char group[8];
            char size[12];
            char mtime[12];
            char checksum[8];   // from 148 to 156
            char type;
            char linkname[100];
            char _padding[255];

            unsigned filenameSize() const
            {
                auto sEnd = (const char*)memchr(name, '\0', sizeof(name));
                return sEnd != nullptr ? sEnd - name : sizeof(name);
            }
            uint64_t decodeSize() const { return Parser::Atoi8<uint64_t>(std::string_view(size, sizeof(size))); }

            bool validate() const
            {
                uint32_t sSum = 0;
                const unsigned char* sPtr = (unsigned char*)name;
                for (unsigned i = 0; i < sizeof(header); i++)
                {
                    // walk over checksum as it filled with spaces
                    unsigned x = (i >= 148 and i < 156) ? ' ' : sPtr[i];
                    sSum += x;
                }
                auto sChecksum = Parser::Atoi8<uint32_t>(std::string_view(checksum, sizeof(checksum)));
                return sSum == sChecksum;
            }
        };

    private:
        std::map<std::string, std::string_view> m_Index;

    public:
        using NotFound = std::invalid_argument;
        using BadInput = std::invalid_argument;

        Tar(std::string_view aData)
        {
            while (aData.size() >= sizeof(header))
            {
                header* sHeaderPtr = (header*)aData.data();

                if (std::all_of(sHeaderPtr->name, sHeaderPtr->name + sizeof(header::name), [](char t){ return t == '\0'; }))
                    return;
                if (!sHeaderPtr->validate())
                    throw BadInput("Tar::BadInput: bad checksum");

                aData.remove_prefix(sizeof(header));
                if (sHeaderPtr->type != '0' and sHeaderPtr->type != '\0')   // skip not supported header
                    continue;

                const auto sName = std::string(sHeaderPtr->name, sHeaderPtr->filenameSize());
                const auto sSize = sHeaderPtr->decodeSize();
                const auto sPadding = sSize > 0 ? 512 - (sSize % 512) : 0;
                if (aData.size() < sSize + sPadding)
                    throw BadInput("Tar::BadInput: incomplete file data");
                const auto sData = std::string_view(aData.data(), sSize);
                m_Index[sName] = sData;
                aData.remove_prefix(sSize + sPadding);
            }
        }

        std::string_view get(const std::string& aName) const
        {
            auto sIt = m_Index.find(aName);
            if (sIt == m_Index.end())
                throw NotFound("Tar::FileNotFound: " + aName);
            return sIt->second;
        }

        size_t size() const { return m_Index.size(); }
    };
} // namespace resource
