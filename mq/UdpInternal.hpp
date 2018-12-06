#pragma once

#include <chrono>
#include <iostream>
#include <thread>
#include <MessageQueue.hpp>

namespace MQ::UDP
{
    using udp = asio::ip::udp;

    struct Config
    {
        std::string bind_host;
        uint16_t    bind_port = 0;

        std::string peer_host;
        uint16_t    peer_port = 0;

        udp::endpoint bind() const { return udp::endpoint(asio::ip::address::from_string(bind_host), bind_port); }
        udp::endpoint peer() const { return udp::endpoint(asio::ip::address::from_string(peer_host), peer_port); }
    };

    enum : uint8_t
    {
        FLAG_STOP = 1,
    };

    struct Header
    {
        uint64_t min_serial = 0; // first serial in task queue on server
        uint64_t serial = 0;
        uint64_t crc    = 0;
        uint16_t size   = 0;
        uint8_t  flag   = 0;
    };

    struct Reply
    {
        uint64_t serial = 0;
        uint8_t  flag   = 0;
    };

    uint8_t flag(const std::atomic_bool& aStop)
    {
        return aStop == true ? FLAG_STOP : (uint8_t)0;
    }
}
