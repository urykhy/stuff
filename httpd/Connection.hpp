#pragma once

#include <string>
#include "Parser.hpp"
#include <networking/EPoll.hpp>
#include <networking/TcpSocket.hpp>
#include <networking/TcpListener.hpp>
#include <threads/SafeQueue.hpp>

namespace httpd
{
    struct Connection : Util::EPoll::HandlerFace, std::enable_shared_from_this<Connection>
    {
        using SharedPtr = std::shared_ptr<Connection>;
        using WeakPtr   = std::weak_ptr<Connection>;

        enum  UserResult { DONE, ASYNC };
        using Handler   = std::function<UserResult(SharedPtr, const Request&)>;

    private:
        Tcp::Socket  m_Socket;
        Util::EPoll* m_EPoll;
        Parser       m_Parser;
        Handler      m_Handler;

        const size_t  WRITE_OUT_LIMIT = 1 * 1024 * 1024;
        const size_t  TASK_LIMIT = 100;
        const ssize_t BUFFER_SIZE = 128 * 1024;
        std::atomic_bool m_Error{false};

        Threads::SafeQueue<Request> m_Incoming;
        uint64_t m_Done{0};
        std::atomic_bool m_Busy{false};

        uint64_t queueSize() const { return m_Incoming.count() - m_Done; }

        void process_request()
        {
            if (m_Error)
                return;

            m_Busy = true;
            while(true)
            {
                auto sItem = m_Incoming.try_get();
                if (!sItem)
                {
                    m_Busy = false;
                    return;
                }
                auto sResult = m_Handler(shared_from_this(), *sItem);
                // if async call -> return in busy state, wait for notify()
                if (sResult == UserResult::ASYNC)
                    return;
                // normal call. can process next request
                m_Done++;
            }
        }

        mutable std::mutex m_Write;
        std::string m_WriteOut;

        size_t writeOutSize() const
        {
            std::unique_lock<std::mutex> lk(m_Write);
            return m_WriteOut.size();
        }

    public:

        Connection(Util::EPoll* aEPoll, Tcp::Socket&& aSocket, Handler aHandler)
        : m_Socket(std::move(aSocket))
        , m_EPoll(aEPoll)
        , m_Parser([this](Request& a){ m_Incoming.insert(a); })
        , m_Handler(aHandler)
        {
            m_WriteOut.reserve(WRITE_OUT_LIMIT);
        }

        int get_fd() const override { return m_Socket.get_fd(); }

        // call from any thread to write response
        void write(const std::string& aData, bool aForceBuffer = false)
        {
            if (m_Error)
                return;

            std::unique_lock<std::mutex> lk(m_Write);
            const bool sIdle = m_WriteOut.empty();
            if (sIdle)
            {
                if (aForceBuffer or queueSize() > 1)
                {
                    m_WriteOut.append(aData);
                    m_EPoll->schedule(shared_from_this(), 1);
                    return;
                }
                ssize_t sSize = 0;
                try
                {
                    sSize = m_Socket.write(aData.data(), aData.size());
                }
                catch (...)
                {
                    on_error(get_fd());
                    m_EPoll->post([p = shared_from_this()](Util::EPoll* ptr) {
                        ptr->erase(p->get_fd());
                    });
                    return;
                }
                if (sSize == (ssize_t)aData.size())
                    return;
                m_WriteOut.append(aData.substr(sSize > 0 ? sSize : 0));   // if EAGAIN - sSize < 0
                m_EPoll->schedule(shared_from_this(), 1);
            } else {
                m_WriteOut.append(aData);
            }
        }

        // called if async request processed
        void notify()
        {
            m_EPoll->post([p = shared_from_this()](Util::EPoll* ptr) {
                p->m_Done++;
                p->process_request();
            });
        }

        Result on_read(int) override
        {
            if (m_Error)
                return CLOSE;

            const uint64_t sQueueSize = queueSize();
            // do not read if queue already have requests or writeout queue busy
            if (sQueueSize > TASK_LIMIT or writeOutSize() > WRITE_OUT_LIMIT)
                return RETRY;       // retry in 1 ms

            ssize_t sSize = 0;
            void* sBuffer = alloca(BUFFER_SIZE);
            while (1)
            {
                sSize = m_Socket.read(sBuffer, BUFFER_SIZE);
                if (sSize == 0)
                    return CLOSE;
                if (sSize < 0)      // EAGAIN: wait for next event
                    break;
                ssize_t sUsed = m_Parser.consume((char*)sBuffer, sSize);
                if (sUsed < sSize)  // parser problem
                    return CLOSE;
                if (sQueueSize < m_Incoming.count()) // got new request(s)
                    break;
            }

            if (!m_Busy and !m_Incoming.idle())
                process_request();
            return sSize < BUFFER_SIZE ? OK : RETRY;
        }

        Result on_write(int) override
        {
            if (m_Error)
                return CLOSE;

            std::unique_lock<std::mutex> lk(m_Write);
            if (m_WriteOut.empty())
                return OK;
            ssize_t sSize = m_Socket.write(m_WriteOut.data(), m_WriteOut.size());
            if (sSize < 0) // eagain, wait for next event
                return OK;
            if (sSize > 0)
                m_WriteOut.erase(0, sSize);
            if (m_WriteOut.size() < WRITE_OUT_LIMIT and m_WriteOut.capacity() > WRITE_OUT_LIMIT)
                m_WriteOut.reserve(WRITE_OUT_LIMIT);
            return OK;
        }

        Result on_timer(int) override
        {   // timer used only to kick write out. so we not need timer_id
            return on_write(0);
        }

        void on_error(int) override { m_Error = true; }
        virtual ~Connection() {}
    };

    template<class H>
    inline auto Create(Util::EPoll *aEPoll, uint16_t aPort, H& aRouter)
    {   // create listener
        return std::make_shared<Tcp::Listener>(aEPoll, aPort, [aEPoll, &aRouter](Tcp::Socket&& aSocket) mutable
        {   // on new connection we create Connection class
            return std::make_shared<Connection>(aEPoll, std::move(aSocket), [&aRouter](Connection::SharedPtr aPeer, const Request& aRequest) mutable
            {   // and once we got request - pass one to router
                return aRouter(aPeer, aRequest);
            });
        });
    }

} // namespace httpd