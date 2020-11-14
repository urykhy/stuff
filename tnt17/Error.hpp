#pragma once

namespace tnt17 {

    struct NetworkError : std::runtime_error
    {
        NetworkError(boost::system::error_code ec)
        : std::runtime_error("Network error: " + ec.message())
        {}
        NetworkError(boost::system::errc::errc_t ec)
        : std::runtime_error("Network error: " + boost::system::errc::make_error_code(ec).message())
        {}
    };
    struct TntError : std::invalid_argument
    {
        TntError(const std::string& e)
        : std::invalid_argument("Tnt error: " + e)
        {}
    };
} // namespace tnt17