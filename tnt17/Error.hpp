#pragma once

namespace tnt17
{
    struct NetworkError   : std::runtime_error { NetworkError (boost::system::error_code ec) : std::runtime_error("network error: " + ec.message()) {}};
    struct ProtocolError  : std::runtime_error { ProtocolError(const std::string& e) : std::runtime_error("protocol error: " + e) {}};
    struct RemoteError    : std::runtime_error { RemoteError  (const std::string& e) : std::runtime_error("remote error: " + e) {}};
}