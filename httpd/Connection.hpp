#pragma once

#include <string>

#include <networking/TcpConnection.hpp>

#include "Parser.hpp"

namespace httpd {

    struct Server
    {
        using Request = httpd::Request;
        using Parser  = httpd::Parser;

        static constexpr size_t  WRITE_BUFFER_SIZE = 1 * 1024 * 1024; // output buffer size
        static constexpr size_t  TASK_LIMIT        = 100;             // max parsed tasks in queue
        static constexpr ssize_t READ_BUFFER_SIZE  = 128 * 1024;      // read buffer size
    };

    using Connection = Tcp::Connection<Server>;

    struct Client
    {
        using Request = httpd::Response;
        using Parser  = httpd::Parser;

        static constexpr size_t  WRITE_BUFFER_SIZE = 1 * 1024 * 1024; // output buffer size
        static constexpr size_t  TASK_LIMIT        = 100;             // max parsed tasks in queue
        static constexpr ssize_t READ_BUFFER_SIZE  = 128 * 1024;      // read buffer size
    };

    using ClientConnection = Tcp::Connection<Client>;

    template <class H>
    inline auto Create(Util::EPoll* aEPoll, uint16_t aPort, H& aRouter)
    {                                                                                                                                                  // create listener
        return std::make_shared<Tcp::Listener>(aEPoll, aPort, [aEPoll, &aRouter](Tcp::Socket&& aSocket) mutable {                                      // on new connection we create Connection class
            return std::make_shared<Connection>(aEPoll, std::move(aSocket), [&aRouter](Connection::SharedPtr aPeer, const Request& aRequest) mutable { // and once we got request - pass one to router
                return aRouter(aPeer, aRequest);                                                                                                       // process request with router
            });
        });
    }

} // namespace httpd