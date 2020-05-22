#pragma once

#include "EPoll.hpp"
#include "TcpSocket.hpp"

namespace Tcp
{
    struct Listener : Util::EPoll::HandlerFace, std::enable_shared_from_this<Listener>
    {
        using Handler = std::function<Util::EPoll::HandlerPtr(Tcp::Socket&&)>;

    private:
        Util::EPoll* m_EPoll;
        Socket  m_Socket;
        Handler m_Handler;

    public:
        Listener(Util::EPoll* aEPoll, int aPort, Handler aHandler)
        : m_EPoll(aEPoll)
        , m_Handler(aHandler)
        {
            m_Socket.set_reuse_port();
            m_Socket.set_nonblocking();
            m_Socket.bind(aPort);
            m_Socket.listen();
        }

        void start()
        {
            m_EPoll->post([p = shared_from_this()](Util::EPoll* ptr) { ptr->insert(p->m_Socket.get_fd(), EPOLLIN, p); });
        }

        virtual Result on_read(int)
        {
            int sFd = 0;
            do
            {
                sFd = m_Socket.accept();
                if (sFd > 0)
                {
                    Socket sSocket(sFd);
                    sSocket.set_nonblocking();
                    auto sNew = m_Handler(std::move(sSocket));
                    m_EPoll->insert(sFd, EPOLLIN, sNew);
                }
            } while (sFd > 0);
            return Result::OK;
        }
        virtual Result on_write(int) { return Result::OK; }
        virtual void on_error(int) {}
        virtual ~Listener() {}
    };
} // namespace Tcp