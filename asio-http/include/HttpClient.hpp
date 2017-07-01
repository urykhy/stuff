
#pragma once

#include <boost/aligned_storage.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/utility/string_ref.hpp>
#include <memory>
#include <mutex>

#include <Logger.hpp>
#include <Parser.hpp>
#include <Expected.hpp>

namespace HTTP {
    namespace asio = boost::asio;
    using boost::asio::ip::tcp;
    using namespace asio::ip;

    struct HttpQuery
    {
        ResponseParser parser;
        const std::string query;

        using Result = ResponseParser;
        Util::Future<Result> result;
        using Handler = std::function<void(Util::Future<Result>&)>;
        Handler handler;

        HttpQuery(const std::string& q, Handler h) : query(q), handler(h) { }

        // API from query runner
        void on_success() { result.set_value(std::move(parser)); }
        void on_error(const std::string& e) { result.set_error(make_exception_ptr(std::runtime_error(e))); }
        void invoke_cb() { handler(result); }
        bool keep_alive = 0;

        bool process(const std::string& read_buffer, size_t length) // return true if done, false = read_more
        {
            if (!parser(read_buffer.data(), length)) {
                on_error(parser.last_error());
                return true;
            }
            if (parser.is_done()) {
                keep_alive = parser.keep_alive;
                on_success();
                return true;
            } else {
                return false;
            }
        }

    };
    using HttpQuery_var = std::shared_ptr<HttpQuery>;

#include <boost/asio/yield.hpp>

    struct Query : public boost::asio::coroutine, public std::enable_shared_from_this<Query>
    {
        const tcp::endpoint endpoint;
        tcp::socket socket;
        HttpQuery_var current;
        enum {read_size = 1024*4};
        std::string read_buffer;

        // if network error
        void on_error(const std::string& e) {
            current->result.set_error(make_exception_ptr(std::runtime_error(e)));
            on_done();
        }

        void on_done() {
            current->invoke_cb();
            close();
        }

        void close() {
            boost::system::error_code ec;
            socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
        }

    public:

        Query(asio::io_service& io_, const std::string& host, int port, HttpQuery_var q)
        : endpoint(address::from_string(host), port),
        socket(io_),
        current(q)
        {
            read_buffer.resize(read_size);
        }

        void run() {
            socket.async_connect(endpoint, [p=this->shared_from_this()](auto error) { p->operator()(error); });
        }

        void operator()(boost::system::error_code ec = boost::system::error_code(),
                        std::size_t length = 0)
        {
            if (!ec)
            {
                reenter (this)
                {
                    TRACE("connected");
                    yield asio::async_write(socket,
                                            asio::buffer(&current->query[0], current->query.size()),
                                            [p=shared_from_this()](auto error, auto len __attribute__((unused))){ p->operator()(error); });
                    while(true)
                    {
                        TRACE("reading reply");
                        yield socket.async_read_some(asio::buffer(&read_buffer[0], read_buffer.size()),
                                                     [p=shared_from_this()](auto error, auto len){ p->operator()(error, len); });
                        TRACE("got " << length << " bytes");
                        if (current->process(read_buffer, length)) // processing done
                        {
                            on_done();
                            return;
                        }
                    }
                }
            } else {
                on_error(ec.message());
            }
        }
    };

#include <boost/asio/unyield.hpp>

    struct ClientBase {
        asio::io_service io_service;

        void run() {
            boost::asio::io_service::work work(io_service);
            log4cxx::NDC ndc("io");
            TRACE("run");
            io_service.run();
        }
        void term() {
            io_service.stop();
            TRACE("terminated");
        }
    };

}

