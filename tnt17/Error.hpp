#pragma once

namespace tnt17 {
    struct NetworkError : std::runtime_error
    {
        NetworkError(boost::system::error_code ec)
        : std::runtime_error("network error: " + ec.message())
        {}
        NetworkError(boost::system::errc::errc_t ec)
        : std::runtime_error("network error: " + boost::system::errc::make_error_code(ec).message())
        {}
    };
    struct TntError : std::invalid_argument
    {
        TntError(const std::string& e)
        : std::invalid_argument("tnt error: " + e)
        {}
    };
} // namespace tnt17