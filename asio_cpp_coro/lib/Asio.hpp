#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

namespace AsioHttp {
    namespace this_coro = ba::this_coro;
    namespace bs        = boost::system;

    using tcp = boost::asio::ip::tcp;

} // namespace AsioHttp
