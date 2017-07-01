
#pragma once
#include <HttpClient.hpp>
#include <TaskQueue.hpp>

namespace HTTP
{

#include <boost/asio/yield.hpp>

    struct AliveQuery : public std::enable_shared_from_this<AliveQuery>
    {
        using Queue = Util::AsioTaskQueue<HttpQuery_var>;
    private:

        enum {ka_timeout = 4};
        enum {read_size = 1024*4};

        const tcp::endpoint endpoint;
        tcp::socket socket;
        std::string read_buffer;

        time_t last = 0;
        std::unique_ptr<boost::asio::coroutine> coro;
        std::unique_ptr<boost::asio::coroutine> w_coro;
        HttpQuery_var current;
        Queue& queue;

        void fork_read() {
            coro = std::make_unique<boost::asio::coroutine>();
            socket.get_io_service().post([p=shared_from_this()](){ p->operator()(); });
        }

        void close() {
            boost::system::error_code ec;
            socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
        }

        void on_error(const std::string& e) {
            close();
            if (current)
            {
                current->result.set_error(make_exception_ptr(std::runtime_error(e)));
                current->invoke_cb();
                current.reset();
                resume();
            } else {
                INFO("Idle socket closed: " << e);
            }
        }

        void on_done() {
            current->invoke_cb();
            current.reset();
            resume();
        }

        void resume() {
            queue.wait([p=shared_from_this()](auto task){
                p->spawn(task);
            });
        }

        void spawn(HttpQuery_var& q) {
            current = q;
            w_coro = std::make_unique<boost::asio::coroutine>();
            write_l();
        }

        void write_l()
        {
            log4cxx::NDC ndc("write");
            reenter (w_coro.get())
            {
                TRACE("last is " << time(0) - last << " seconds away");
                if (!socket.is_open() || last + ka_timeout <= time(0)) {
                    if (!socket.is_open()) {
                        TRACE("open new connection");
                    } else {
                        close();
                        TRACE("keep alive timeout, reconnecting");
                    }
                    yield socket.async_connect(endpoint,
                                               [p=shared_from_this()](auto error) {
                                                   if (!error) {
                                                       p->write_l();                           // and perform actual writing
                                                   } else {
                                                       p->on_error(error.message());
                                                   }
                                               });

                    TRACE("connected");
                    fork_read();
                }
                TRACE("writing query");
                yield asio::async_write(socket,
                                        asio::buffer(&current->query[0], current->query.size()),
                                        [p=shared_from_this()](auto error, auto len __attribute__((unused))) {
                                            if (error) {
                                                p->on_error(error.message());
                                            }
                                            // w_coro stopped here
                                        });
            }
        }

    public:

        AliveQuery(asio::io_service& io_, const std::string& host, int port, Queue& q)
        : endpoint(address::from_string(host), port),
        socket(io_),
        queue(q)
        {
            read_buffer.resize(read_size);
        }

        void start() {
            resume();
        }

        void operator()(boost::system::error_code ec = boost::system::error_code(),
                        std::size_t length = 0)
        {
            log4cxx::NDC ndc("read");
            if (!ec)
            {
                reenter (coro.get())
                {
                    while (socket.is_open())
                    {
                        TRACE("reading reply");
                        yield socket.async_read_some(asio::buffer(&read_buffer[0], read_buffer.size()),
                                                     [p=shared_from_this()](auto error, auto len){ p->operator()(error, len); });
                        last = time(0);
                        assert (current);
                        if (current->process(read_buffer, length)) // processing done
                        {
                            const bool ka = current->keep_alive;
                            if (!ka) {
                                TRACE("no keep alive, close socket");
                                close();
                            }
                            on_done();
                        }
                    }
                }
            } else {
                if (ec != asio::error::operation_aborted) {
                    on_error(ec.message());
                } else {
                    TRACE("operation aborted");
                }
            }
        }
    };

#include <boost/asio/unyield.hpp>

}

