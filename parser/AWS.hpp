#pragma once

#include <endian.h>

#include <container/Stream.hpp>
#include <format/Hex.hpp>
#include <hash/CRC32.hpp>

namespace Parser::AWS {

    // https://docs.aws.amazon.com/AmazonS3/latest/API/RESTSelectObjectAppendix.html
    std::string Stream(const std::string_view aData)
    {
        std::string           sStr;
        Container::imemstream sStream(aData);

        struct Prelude
        {
            uint32_t total   = 0;
            uint32_t headers = 0;

            void read(Container::imemstream& aStream)
            {
                aStream.read(total);
                aStream.read(headers);
                uint32_t sRemoteCRC = 0;
                aStream.read(sRemoteCRC);

                const auto sLocalCRC = Hash::CRC32::sum(total, headers);
                if (sLocalCRC != be32toh(sRemoteCRC))
                    throw std::invalid_argument("Parser::AWS invalid prelude checksum");

                headers = be32toh(headers);
                total   = be32toh(total);
            }
            uint32_t data_size() const
            {
                return total - headers - 16;
            }
        };

        struct KV
        {
            std::string_view name;
            std::string_view value;

            void read(Container::imemstream& aStream)
            {
                uint8_t sHSize = 0;
                aStream.read(sHSize);
                name           = aStream.substring(sHSize);
                uint8_t sSeven = 0;
                aStream.read(sSeven);
                if (sSeven == '7')
                    throw std::invalid_argument("Parser::AWS invalid kv entry");
                uint16_t sVSize = 0;
                aStream.read(sVSize);
                value = aStream.substring(be16toh(sVSize));
            }
        };

        enum
        {
            CRC_SIZE = 4
        };
        while (!sStream.eof()) {
            bool    sRecords = false;
            size_t  sOffset  = sStream.offset();
            Prelude sPre;
            sPre.read(sStream);

            // check message CRC
            [&sStream, sOffset, &sPre]() {
                const auto     sMessage  = sStream.substr_at(sOffset, sPre.total - CRC_SIZE);
                const uint32_t sLocalCRC = Hash::CRC32::sum(sMessage);

                uint32_t               sRemoteCRC    = 0;
                const std::string_view sStrRemoteCRC = sStream.substr_at(sOffset + sMessage.size(), CRC_SIZE);
                memcpy(&sRemoteCRC, sStrRemoteCRC.data(), CRC_SIZE);
                if (sLocalCRC != be32toh(sRemoteCRC))
                    throw std::invalid_argument("Parser::AWS invalid message checksum");
            }();

            Container::imemstream sHeader(sStream.substring(sPre.headers));
            while (!sHeader.eof()) {
                KV sKV;
                sKV.read(sHeader);
                if (sKV.name == ":event-type" and sKV.value == "Records")
                    sRecords = true;
                if (sKV.name == ":error-message")
                    throw std::invalid_argument("Parser::AWS " + std::string(sKV.value));
            }
            if (sRecords) {
                sStr.append(sStream.substring(sPre.data_size()));
            } else {
                sStream.substring(sPre.data_size());
            }
            sStream.skip(CRC_SIZE); // CRC already processed
        }

        return sStr;
    }
} // namespace Parser::AWS