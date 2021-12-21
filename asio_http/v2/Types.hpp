#pragma once

#include "../Router.hpp" // asio types

#include <unsorted/Enum.hpp>
#include <unsorted/Log4cxx.hpp>

namespace asio_http::v2 {

    inline log4cxx::LoggerPtr sLogger = Logger::Get("http");

    enum class Type : uint8_t
    {
        DATA          = 0,
        HEADERS       = 1,
        SETTINGS      = 4,
        GOAWAY        = 7,
        WINDOW_UPDATE = 8,
        CONTINUATION  = 9,
    };
    static const std::map<Type, std::string> sTypeNames = {
        {Type::DATA, "data"},
        {Type::HEADERS, "headers"},
        {Type::SETTINGS, "settings"},
        {Type::GOAWAY, "goaway"},
        {Type::WINDOW_UPDATE, "window"},
        {Type::CONTINUATION, "continuation"}};
    _DECLARE_ENUM_TO_STRING(Type, sTypeNames)

    enum Setting : uint16_t
    {
        HEADER_TABLE_SIZE      = 0x1,
        MAX_CONCURRENT_STREAMS = 0x3,
        INITIAL_WINDOW_SIZE    = 0x4,
        MAX_FRAME_SIZE         = 0x5,
        MAX_HEADER_LIST_SIZE   = 0x6,
    };

    constexpr size_t MIN_FRAME_SIZE            = 4096;
    constexpr size_t MAX_STREAM_EXCLUSIVE      = 131072;
    constexpr size_t DEFAULT_HEADER_TABLE_SIZE = 4096;
    constexpr size_t DEFAULT_MAX_FRAME_SIZE    = 16384;
    constexpr size_t DEFAULT_WINDOW_SIZE       = 65535;

    enum Flags : uint8_t
    {
        ACK_SETTINGS = 0x1,
        END_STREAM   = 0x1,
        END_HEADERS  = 0x4,
        PADDED       = 0x8,
        PRIORITY     = 0x20,
    };
    static const std::map<Flags, std::string> sFlagsNames = {
        {END_STREAM, "end_stream"},
        {END_HEADERS, "end_headers"},
        {PADDED, "padded"},
        {PRIORITY, "priority"}};
    _DECLARE_SET_TO_STRING(Flags, sFlagsNames)
    _DECLARE_ENUM_OPS(Flags)

    struct Header
    {
        uint32_t size : 24 = {};
        Type     type      = {};
        Flags    flags     = {};
        uint32_t stream    = {};

        void to_host()
        {
            size   = be32toh(size << 8);
            stream = be32toh(stream);
        }
        void to_net()
        {
            size   = htobe32(size) >> 8;
            stream = htobe32(stream);
        }
    } __attribute__((packed));

    struct SettingVal
    {
        uint16_t key   = {};
        uint32_t value = {};

        void to_host()
        {
            key   = be16toh(key);
            value = be32toh(value);
        }
    } __attribute__((packed));

    struct CoroState
    {
        beast::error_code   ec;
        asio::yield_context yield;
    };
} // namespace asio_http::v2