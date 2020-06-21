#pragma once

#include <threads/SafeQueue.hpp>

#include "EPoll.hpp"
#include "TcpListener.hpp"
#include "TcpSocket.hpp"

namespace Tcp {

    template <class P>
    struct Connection
    : Util::EPoll::HandlerFace,
      std::enable_shared_from_this<Connection<P>>
    {
        using Request   = typename P::Request;
        using Parser    = typename P::Parser;
        using SharedPtr = std::shared_ptr<Connection<P>>;
        using WeakPtr   = std::weak_ptr<Connection<P>>;

        enum class UserResult
        {
            DONE,
            ASYNC,
            CLOSE
        };
        using Handler        = std::function<UserResult(SharedPtr, const Request&)>;
        using ConnectHandler = std::function<void(int)>;    // called with 0 or error code

    private:
        Tcp::Socket                 m_Socket;
        Util::EPoll*                m_EPoll;
        Parser                      m_Parser;
        Handler                     m_Handler;
        ConnectHandler              m_ConnectHandler;
        Threads::SafeQueue<Request> m_Incoming;

        uint64_t         m_Done{0};
        std::atomic_bool m_Busy{false};
        std::atomic_bool m_Error{false};
        std::atomic_bool m_Closing{false};
        std::atomic_int  m_Connected{EPIPE};

        enum : int
        {
            TIMER_ID_CONNECTING = 2
        };

        uint64_t queueSize() const { return m_Incoming.count() - m_Done; }

        void process_request()
        {
            if (m_Error)
                return;

            m_Busy = true;
            while (true) {
                auto sItem = m_Incoming.try_get();
                if (!sItem) {
                    m_Busy = false;
                    if (m_Closing and writeOutSize() == 0)
                        self_close();
                    return;
                }
                if (!sItem->m_KeepAlive)
                    m_Closing = true;
                auto sResult = m_Handler(this->shared_from_this(), *sItem);
                // if async call -> return in busy state, wait for notify()
                if (sResult == UserResult::ASYNC)
                    return;
                if (sResult == UserResult::CLOSE)
                    m_Closing = true;
                // normal call. can process next request
                m_Done++;
            }
        }

        mutable std::mutex m_Write;
        std::string        m_WriteOut;

        size_t writeOutSize() const
        {
            std::unique_lock<std::mutex> lk(m_Write);
            return m_WriteOut.size();
        }

        void self_close()
        {
            m_EPoll->post([p = this->shared_from_this()](Util::EPoll* ptr) { ptr->erase(p->get_fd()); });
        }

    public:
        Connection(Util::EPoll* aEPoll, Tcp::Socket&& aSocket, Handler aHandler)
        : m_Socket(std::move(aSocket))
        , m_EPoll(aEPoll)
        , m_Parser([this](Request& a) { m_Incoming.insert(a); })
        , m_Handler(aHandler)
        {
            m_WriteOut.reserve(P::WRITE_BUFFER_SIZE);
        }

        void connect(uint32_t aRemote, uint16_t aPort, time_t aTimeoutMS, ConnectHandler aHandler)
        {
            m_Socket.set_nonblocking();
            m_Connected      = EINPROGRESS;
            m_ConnectHandler = aHandler;
            m_EPoll->post([this, p = this->shared_from_this(), aRemote, aPort, aTimeoutMS](Util::EPoll*) {
                m_EPoll->insert(get_fd(), EPOLLOUT | EPOLLIN, p);
                m_Socket.connect(aRemote, aPort);
                m_EPoll->schedule(this->shared_from_this(), aTimeoutMS, TIMER_ID_CONNECTING);
            });
        }

        int is_connected() const // return error code or 0 if connected
        {
            return m_Connected;
        }

        int get_fd() const override { return m_Socket.get_fd(); }

        // call from any thread to write response
        void write(const std::string& aData, bool aForceBuffer = false)
        {
            if (m_Error)
                return;

            std::unique_lock<std::mutex> lk(m_Write);
            const bool                   sIdle = m_WriteOut.empty();
            if (sIdle) {
                if (aForceBuffer or queueSize() > 1) {
                    m_WriteOut.append(aData);
                    m_EPoll->schedule(this->shared_from_this(), 1);
                    return;
                }
                ssize_t sSize = 0;
                try {
                    sSize = m_Socket.write(aData.data(), aData.size());
                } catch (...) {
                    on_error(get_fd());
                    self_close();
                    return;
                }
                if (sSize == (ssize_t)aData.size())
                    return;
                m_WriteOut.append(aData.substr(sSize > 0 ? sSize : 0)); // if EAGAIN - sSize < 0
                m_EPoll->schedule(this->shared_from_this(), 1);
            } else {
                m_WriteOut.append(aData);
            }
        }

        // called if async request processed
        void notify(UserResult sResult)
        {
            switch (sResult) {
            case UserResult::CLOSE:
                m_Closing = true;
            case UserResult::DONE:
                m_EPoll->post([p = this->shared_from_this()](Util::EPoll* ptr) { p->m_Done++; p->process_request(); });
                break;
            default:
                break; // ASYNC is no op
            }
        }

        Result on_read(int) override
        {
            if (m_Error)
                return Result::CLOSE;
            if (m_Closing)
                return Result::OK; // no more reads

            const uint64_t sQueueSize = queueSize();
            // do not read if queue already have requests or writeout queue busy
            if (sQueueSize > P::TASK_LIMIT or writeOutSize() > P::WRITE_BUFFER_SIZE)
                return Result::RETRY; // retry in 1 ms

            ssize_t sSize   = 0;
            void*   sBuffer = alloca(P::READ_BUFFER_SIZE);
            while (1) {
                sSize = m_Socket.read(sBuffer, P::READ_BUFFER_SIZE);
                if (sSize == 0)
                    return Result::CLOSE;
                if (sSize < 0) // EAGAIN: wait for next event
                    break;
                ssize_t sUsed = m_Parser.consume((char*)sBuffer, sSize);
                if (sUsed < sSize) // parser problem
                    return Result::CLOSE;
                if (sQueueSize < m_Incoming.count()) // got new request(s)
                    break;
            }

            if (!m_Busy and !m_Incoming.idle())
                process_request();
            if (m_Closing and !m_Busy and writeOutSize() == 0)
                return Result::CLOSE;
            return sSize < P::READ_BUFFER_SIZE ? Result::OK : Result::RETRY;
        }

        Result on_write(int) override
        {
            if (m_Connected == EINPROGRESS) {
                m_Connected = m_Socket.get_error();
                m_ConnectHandler(m_Connected);
                if (m_Connected != 0)
                    throw Util::CoreSocket::Error("fail to connect", m_Connected);
                return Result::OK;
            }

            if (m_Error)
                return Result::CLOSE;

            std::unique_lock<std::mutex> lk(m_Write);
            if (m_WriteOut.empty())
                return Result::OK;
            ssize_t sSize = m_Socket.write(m_WriteOut.data(), m_WriteOut.size());
            if (sSize < 0) // eagain, wait for next event
                return Result::OK;
            if (sSize > 0)
                m_WriteOut.erase(0, sSize);
            if (m_WriteOut.size() < P::WRITE_BUFFER_SIZE and m_WriteOut.capacity() > P::WRITE_BUFFER_SIZE)
                m_WriteOut.reserve(P::WRITE_BUFFER_SIZE);
            if (m_WriteOut.empty() and m_Closing and !m_Busy)
                return Result::CLOSE;
            return Result::OK;
        }

        Result on_timer(int aTimerID) override
        {
            if (aTimerID == TIMER_ID_CONNECTING) {
                if (m_Connected == EINPROGRESS) {
                    m_Connected = ETIMEDOUT;
                    m_ConnectHandler(m_Connected);
                    throw Util::CoreSocket::Error("fail to connect", ETIMEDOUT);
                }
                return Result::OK;
            }
            // it's a write-out message
            return on_write(0);
        }

        void on_error(int) override { m_Error = true; }
        virtual ~Connection() {}
    };

} // namespace Tcp