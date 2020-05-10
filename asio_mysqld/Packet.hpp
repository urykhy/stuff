#pragma once

#include <stdint.h>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

#include <container/Stream.hpp>

namespace asio_mysql
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace net = boost::asio;

    enum
    {
        CMD_QUIT  = 1,
        CMD_QUERY = 3,

        MYSQL_TYPE_LONG    = 3,
        MYSQL_TYPE_STRING  = 254,

        CLIENT_PROTOCOL_41 = 512,
    };

    using binary = Container::binary;

    struct omemstream : Container::omemstream
    {
        using Container::omemstream::write;
        omemstream(Container::binary& aData) : Container::omemstream(aData) {}

        void writeLenEnc(uint64_t aValue) // length encode
        {
            if (aValue < 251) { write(&aValue, 1); }
            else if (aValue < (1 << 16)) { put(0xFC); write(&aValue, 2); }
            else if (aValue < (1 << 24)) { put(0xFD); write(&aValue, 3); }
            else { put(0xFE); write(&aValue, 8); }
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
        imemstream(const Container::binary& aData) : Container::imemstream(aData) {}

        template<class T> void read(T& aData) { read(&aData, sizeof(aData)); }

        void readStringNul(std::string& aStr) {
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

    inline beast::error_code read(beast::tcp_stream& aStream, net::yield_context yield, binary& aBuffer)
    {
        beast::error_code ec;
        uint32_t sSize = 0;
        asio::async_read(aStream, asio::buffer(&sSize, sizeof(sSize)), yield[ec]);
        if (ec)
            return ec;
        sSize &= 0xFFFFFF;  // drop serial
        aBuffer.resize(sSize);
        asio::async_read(aStream, asio::mutable_buffer(aBuffer.data(), sSize), yield[ec]);
        return ec;
    }

    inline beast::error_code write(beast::tcp_stream& aStream, net::yield_context yield, const binary& aBuffer, uint8_t aSequence = 0)
    {
        beast::error_code ec;
        uint32_t sSize = aBuffer.size();
        sSize |= (uint32_t(aSequence) << 24);
        asio::async_write(aStream, asio::buffer(&sSize, sizeof(sSize)), yield[ec]);
        if (ec)
            return ec;
        asio::async_write(aStream, asio::buffer(aBuffer.data(), aBuffer.size()), yield[ec]);
        return ec;
    }

    struct Handshake10
    {
        char        protocol_version    = 10;
        char        server_version[12]  = "asio/mysqld";  // human readable status information. string<NUL>
        uint32_t    connection_id       = 0;
        char        auth_plugin_data[8] = {"\x0\x0\x0\x0\x0\x0\x0"};
        char        filler              = 0;    // 0x00 byte, terminating the first part of a scramble
        uint16_t    capability_flags_1  = 0;    // The lower 2 bytes of the Capabilities Flags
        uint8_t     character_set       = 0x21; // utf8_general_ci
        uint16_t    status_flags        = 0;    //
        uint16_t    capability_flags_2  = 0;    // The upper 2 bytes of the Capabilities Flags
        char        nul                 = 0;    // constant 0x00
        char        reserved[10]        = {"\x0\x0\x0\x0\x0\x0\x0\x0\x0"};
        char        auth_plugin_data_l  = 0;    // Rest of the plugin provided data (scramble). LengthEncodedString

        void serialize(binary& aBinary) const
        {
            aBinary.clear();
            omemstream sStream(aBinary);
            sStream.write(this, sizeof(*this));
        }
    } __attribute__((packed));

    struct HandshakeResponse // 320
    {
        uint16_t client_flag = 0;       // Capabilities Flags, only the lower 16 bits
        uint32_t max_packet_size = 0;   // maximum packet size, 0xFFFFFF max
        std::string username;           // login user name, string<NUL>
        std::string auth;               // authentication response data, string<EOF>

        void parse(binary aBinary)
        {
            imemstream sStream(aBinary);
            sStream.read(client_flag);
            char tmp[3];
            sStream.read(tmp, sizeof(tmp));
            memcpy(&max_packet_size, tmp, sizeof(tmp));
            sStream.readStringNul(username);
            sStream.readStringEof(auth);
        }
    };

    struct OkResponse
    {
        char header = 0x00; // OK packet header
        uint32_t affected_rows = 0;
        uint32_t last_insert_id = 0;
        std::string status;

        void serialize(binary& aBinary) const
        {
            aBinary.clear();
            omemstream sStream(aBinary);
            sStream.put(header);
            sStream.writeLenEnc(affected_rows);
            sStream.writeLenEnc(last_insert_id);
            uint16_t status_flags = 0;  // FIXME: capabilities & CLIENT_TRANSACTIONS
            sStream.write(&status_flags, sizeof(status_flags));
            sStream.write(status.data(), status.size());
        }
    };

    struct EofPacket
    {
        void serialize(binary& aBinary) const
        {
            aBinary.clear();
            omemstream sStream(aBinary);
            sStream.write(uint8_t(0xFE));
            // FIXME: capabilities & CLIENT_PROTOCOL_41
            sStream.write(uint16_t(0)); // number of warnings
            sStream.write(uint16_t(0)); // server status
        }
    };

    struct Command
    {
        uint8_t     command = 0;
        std::string query;

        void parse(const binary& aBinary)
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
        std::string table;              // virtual table name
        std::string name;               // virtual column name
        uint16_t character_set = 0x21;  // utf8_general_ci
        uint32_t column_length = 0;     // maximum length of the field
        uint8_t  column_type   = MYSQL_TYPE_STRING;
        uint16_t column_flags  = 0;     // Column Definition Flags
        uint8_t  decimals      = 0;     // decimals

        void serialize(binary& aBinary) const
        {
            aBinary.clear();
            omemstream sStream(aBinary);
            sStream.writeLenEnc("def"); // catalog
            sStream.writeLenEnc(schema);
            sStream.writeLenEnc(table);
            sStream.writeLenEnc("");    // original table
            sStream.writeLenEnc(name);
            sStream.writeLenEnc("");    // original name
            sStream.write(uint8_t(0x0c));
            sStream.write(character_set);
            sStream.write(column_length);
            sStream.write(column_type);
            sStream.write(column_flags);
            sStream.write(decimals);
            sStream.write(uint16_t(0));
        }
    };

    struct ResultRow
    {
        std::string data;

        ResultRow& append(const std::string& aData)
        {
            omemstream sStream(data);
            sStream.writeLenEnc(aData);
            return *this;
        }

        ResultRow& append() // null
        {
            omemstream sStream(data);
            sStream.write(uint8_t(0xFB));
            return *this;
        }
    };

    struct ResultSet
    {
        std::list<Column> columns;
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
            uint8_t sSerial = 1;
            beast::error_code ec;
            Container::binary sBuffer;

            // number of fields
            omemstream sStream(sBuffer);
            sStream.writeLenEnc(columns.size());
            ec = asio_mysql::write(aStream, yield[ec], sBuffer, sSerial++);
            if (ec) return ec;

            // catalog
            for (auto& x : columns)
            {
                x.serialize(sBuffer);
                ec = asio_mysql::write(aStream, yield[ec], sBuffer, sSerial++);
                if (ec) return ec;
            }

            // EOF marker
            EofPacket sEof;
            sEof.serialize(sBuffer);
            ec = asio_mysql::write(aStream, yield[ec], sBuffer, sSerial++);
            if (ec) return ec;

            // actual data. each row is a packet
            for (auto& x : rows)
            {
                ec = asio_mysql::write(aStream, yield[ec], x.data, sSerial++);
                if (ec) return ec;
            }

            // EOF marker
            sEof.serialize(sBuffer);
            ec = asio_mysql::write(aStream, yield[ec], sBuffer, sSerial++);
            return ec;
        }
    };
}