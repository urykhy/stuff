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

    struct MassClient
    {
        using ClientPtr = std::shared_ptr<ClientConnection>;

        struct Params
        {
            time_t   connect_ms  = 10;
            time_t   timeout_ms  = 3000;
            uint32_t remote_addr = 0;
            uint16_t remote_port = 80;
        };

        struct Query
        {
            std::string request;
            // and pass error code
            std::function<void(int aCode, const Response& aResponse)> callback;
        };

    private:
        Util::EPoll* m_EPoll;
        const Params m_Params;
        ClientPtr    m_Connection;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::list<Query>   m_Queue;
        /*
        struct Timer : Util::EPoll::HandlerFace, std::enable_shared_from_this<Timer>
        {
            RealClient* m_Parent = nullptr;
            Timer(RealClient* aParent) : m_Parent {}

            virtual Result on_read() { return Result::OK; };
            virtual Result on_write() { return Result::OK; }
            virtual void   on_error() {}
            virtual Result on_timer(int aID) { m_Parent->on_timer(); }
            virtual ~Timer() {}
        };

        void on_timer()
        {
            // start timer: m_EPoll->schedule(timer_ptr, timeout_ms_for_1st_query);
            // FIXME: self_close m_Connection since timeout
            // flush(ETIMEDOUT);
        }
*/
        void flush_i(int aCode)
        {
            for (auto& x : m_Queue)
                x.callback(aCode, {});
            m_Queue.clear();
        }

        // got response
        void notify(const Response& aResponse)
        {
            Lock lk(m_Mutex);
            if (m_Queue.empty())
                return;
            auto x = std::move(m_Queue.front());
            m_Queue.pop_front();
            x.callback(0, aResponse);
        }
        // called once connected
        void notify(int aCode)
        {
            Lock lk(m_Mutex);
            if (aCode == 0) {
                // connection ok. write pending requests
                for (auto& x : m_Queue)
                    m_Connection->write(x.request, true);
            } else {
                // connection failed. fire callbacks with error
                flush_i(aCode);
                m_Connection.reset();
            }
        }

        void initiate_i()
        {
            m_Connection = std::make_shared<ClientConnection>(m_EPoll, [this](ClientConnection::SharedPtr aPeer, const Response& aResponse) {
                notify(aResponse);
                return ClientConnection::UserResult::DONE;
            });
            m_Connection->connect(m_Params.remote_addr, m_Params.remote_port, m_Params.connect_ms, [this](int aCode) {
                notify(aCode);
                return;
            });
        }

    public:
        MassClient(Util::EPoll* aEPoll, const Params& aParams)
        : m_EPoll(aEPoll)
        , m_Params(aParams)
        {}

        ~MassClient()
        {
            Lock lk(m_Mutex);
            if (m_Connection)
                m_Connection->self_close();
        }

        void insert(Query&& aQuery, bool aMore = false)
        {
            const int sStatus = is_connected();
            Lock lk(m_Mutex);

            if (sStatus != 0 and sStatus != EINPROGRESS)
                initiate_i();
            if (sStatus == 0)
                m_Connection->write(aQuery.request, aMore);

            m_Queue.push_back(std::move(aQuery));
        }

        int is_connected() const
        {
            Lock lk(m_Mutex);
            return m_Connection ? m_Connection->is_connected() : ENOTCONN;
        }
    };

} // namespace httpd