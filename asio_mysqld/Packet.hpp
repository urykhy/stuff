#pragma once

#include <stdint.h>

#include <string>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

#include <container/Stream.hpp>

namespace asio_mysql {
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace net   = boost::asio;

    enum
    {
        CMD_QUIT  = 1,
        CMD_QUERY = 3,

        MYSQL_TYPE_LONG   = 3,
        MYSQL_TYPE_STRING = 254,

        CLIENT_PROTOCOL_41 = 512,
    };

    struct omemstream : Container::omemstream
    {
        using Container::omemstream::write;
        omemstream() {}

        void writeLenEnc(uint64_t aValue) // length encode
        {
            if (aValue < 251) {
                write(&aValue, 1);
            } else if (aValue < (1 << 16)) {
                put(0xFC);
                write(&aValue, 2);
            } else if (aValue < (1 << 24)) {
                put(0xFD);
                write(&aValue, 3);
            } else {
                put(0xFE);
                write(&aValue, 8);
            }
        }

        void writeLenEnc(const std::string& aStr)
        {
            writeLenEnc(aStr.size());
            write(aStr.data(), aStr.size());
        }
    };

    struct imemstream : Container::imemstream
    {
        using Container::imemstream::read;
        imemstream(const std::string aData)
        : Container::imemstream(aData)
        {
        }

        void readStringNul(std::string& aStr)
        {
            char t = 0;
            do {
                read(&t, sizeof(t));
                if (t != 0)
                    aStr.push_back(t);
            } while (t != 0);
        }

        void readStringEof(std::string& aStr)
        {
            auto sTmp = rest();
            aStr.assign(sTmp.data(), sTmp.size());
        }
    };

    inline beast::error_code read(beast::tcp_stream& aStream, net::yield_context yield, std::string& aBuffer)
    {
        beast::error_code ec;
        uint32_t          sSize = 0;
        asio::async_read(aStream, asio::buffer(&sSize, sizeof(sSize)), yield[ec]);
        if (ec)
            return ec;
        sSize &= 0xFFFFFF; // drop serial
        aBuffer.resize(sSize);
        asio::async_read(aStream, asio::mutable_buffer(aBuffer.data(), sSize), yield[ec]);
        return ec;
    }

    inline beast::error_code write(beast::tcp_stream& aStream, net::yield_context yield, const std::string& aBuffer, uint8_t aSequence = 0)
    {
        beast::error_code ec;
        uint32_t          sSize = aBuffer.size();
        sSize |= (uint32_t(aSequence) << 24);
        asio::async_write(aStream, asio::buffer(&sSize, sizeof(sSize)), yield[ec]);
        if (ec)
            return ec;
        asio::async_write(aStream, asio::buffer(aBuffer.data(), aBuffer.size()), yield[ec]);
        return ec;
    }

    // https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::Handshake
    struct Handshake10
    {
        char     protocol_version    = 10;
        char     server_version[12]  = "asio/mysqld"; // human readable status information. string<NUL>
        uint32_t connection_id       = 0;
        char     auth_plugin_data[8] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        char     filler              = 0;      // 0x00 byte, terminating the first part of a scramble
        uint16_t capability_flags_1  = 0x0200; // The lower 2 bytes of the Capabilities Flags (CLIENT_PROTOCOL_41)
        uint8_t  character_set       = 0x21;   // utf8_general_ci
        uint16_t status_flags        = 0;      //
        uint16_t capability_flags_2  = 0;      // The upper 2 bytes of the Capabilities Flags
        char     ap_data_length      = 13;     // length of auth-plugin-data
        char     reserved[10]        = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        char     ap_data[13]         = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        void serialize(omemstream& sStream) const
        {
            sStream.str().clear();
            sStream.write(this, sizeof(*this));
        }
    } __attribute__((packed));

    struct HandshakeResponse // 41
    {
        uint32_t    client_flag     = 0; // Capabilities
        uint32_t    max_packet_size = 0; // maximum packet size
        uint8_t     character_set   = 0; //
        char        reserved[23]    = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::string username; // login user name, string<NUL>
        std::string auth;     // authentication response data, string<EOF>

        void parse(const std::string& aBinary)
        {
            imemstream sStream(aBinary);
            sStream.read(client_flag);
            sStream.read(max_packet_size);
            sStream.read(character_set);
            sStream.skip(23);
            sStream.readStringNul(username);
            sStream.readStringEof(auth);
        }
    };

    struct AuthSwitchRequest
    {
        uint8_t header          = 0xfe;
        char    plugin_name[22] = "mysql_native_password";
        char    auth_data[21]   = "abcdeabcdeabcdeabcde";

        void serialize(omemstream& aStream) const
        {
            aStream.str().clear();
            aStream.put(header);
            aStream.write(&plugin_name, sizeof(plugin_name));
            aStream.write(&auth_data, sizeof(auth_data));
        }
    };

    struct OkResponse
    {
        char        header         = 0x00; // OK packet header
        uint32_t    affected_rows  = 0;
        uint32_t    last_insert_id = 0;
        uint16_t    status_flags   = 0;
        uint16_t    warnings       = 0;
        std::string status;

        void serialize(omemstream& aStream) const
        {
            aStream.str().clear();
            aStream.put(header);
            aStream.writeLenEnc(affected_rows);
            aStream.writeLenEnc(last_insert_id);
            aStream.write(&status_flags, sizeof(status_flags));
            aStream.write(&warnings, sizeof(warnings));
            aStream.write(status.data(), status.size());
        }
    };

    struct EofPacket
    {
        void serialize(omemstream& aStream) const
        {
            aStream.str().clear();
            aStream.write(uint8_t(0xFE));
            // FIXME: capabilities & CLIENT_PROTOCOL_41
            aStream.write(uint16_t(0)); // number of warnings
            aStream.write(uint16_t(0)); // server status
        }
    };

    struct Command
    {
        uint8_t     command = 0;
        std::string query;

        void parse(const std::string& aBinary)
        {
            imemstream sStream(aBinary);
            sStream.read(command);
            if (command == CMD_QUERY)
                sStream.readStringEof(query);
        }
    };

    struct Column
    {
        std::string schema;
        std::string table;                // virtual table name
        std::string name;                 // virtual column name
        uint16_t    character_set = 0x21; // utf8_general_ci
        uint32_t    column_length = 0;    // maximum length of the field
        uint8_t     column_type   = MYSQL_TYPE_STRING;
        uint16_t    column_flags  = 0; // Column Definition Flags
        uint8_t     decimals      = 0; // decimals

        void serialize(omemstream& aStream) const
        {
            aStream.str().clear();
            aStream.writeLenEnc("def"); // catalog
            aStream.writeLenEnc(schema);
            aStream.writeLenEnc(table);
            aStream.writeLenEnc(""); // original table
            aStream.writeLenEnc(name);
            aStream.writeLenEnc(""); // original name
            aStream.write(uint8_t(0x0c));
            aStream.write(character_set);
            aStream.write(column_length);
            aStream.write(column_type);
            aStream.write(column_flags);
            aStream.write(decimals);
            aStream.write(uint16_t(0));
        }
    };

    struct ResultRow
    {
        std::shared_ptr<omemstream> stream;

        ResultRow()
        : stream(std::make_shared<omemstream>())
        {
        }

        ResultRow& append(const std::string& aData)
        {
            stream->writeLenEnc(aData);
            return *this;
        }

        ResultRow& append() // null
        {
            stream->write(uint8_t(0xFB));
            return *this;
        }
    };

    struct ResultSet
    {
        std::list<Column>    columns;
        std::list<ResultRow> rows;

        Column& add_column(const std::string& aName)
        {
            columns.push_back({});
            columns.back().name = aName;
            return columns.back();
        }

        ResultRow& add_row()
        {
            rows.push_back({});
            return rows.back();
        }

        beast::error_code write(beast::tcp_stream& aStream, net::yield_context yield) const
        {
            uint8_t           sSerial = 1;
            beast::error_code ec;

            // number of fields
            omemstream sStream;
            sStream.writeLenEnc(columns.size());
            ec = asio_mysql::write(aStream, yield[ec], sStream.str(), sSerial++);
            if (ec)
                return ec;

            // catalog
            for (auto& x : columns) {
                x.serialize(sStream);
                ec = asio_mysql::write(aStream, yield[ec], sStream.str(), sSerial++);
                if (ec)
                    return ec;
            }

            // EOF marker
            EofPacket sEof;
            sEof.serialize(sStream);
            ec = asio_mysql::write(aStream, yield[ec], sStream.str(), sSerial++);
            if (ec)
                return ec;

            // actual data. each row is a packet
            for (auto& x : rows) {
                ec = asio_mysql::write(aStream, yield[ec], x.stream->str(), sSerial++);
                if (ec)
                    return ec;
            }

            // EOF marker
            sEof.serialize(sStream);
            ec = asio_mysql::write(aStream, yield[ec], sStream.str(), sSerial++);
            return ec;
        }
    };
} // namespace asio_mysql
