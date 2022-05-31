#include <memory>

#define ASIO_HTTP_LIBRARY_IMPL

#include "API.hpp"
#include "Router.hpp"

// bit includes from API.hpp
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace asio_http {
    using tcp = boost::asio::ip::tcp;
}

#include "Alive.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "v2/Client.hpp"
#include "v2/Server.hpp"