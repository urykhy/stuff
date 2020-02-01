#pragma once

#include <string>
#include "Parser.hpp"
#include <networking/EPoll.hpp>
#include <networking/TcpSocket.hpp>
#include <networking/TcpListener.hpp>
#include <threads/SafeQueue.hpp>

namespace httpd
{
    struct Server : Util::EPoll::HandlerFace, std::enable_shared_from_this<Server>
    {
        using SharedPtr = std::shared_ptr<Server>;
        using WeakPtr   = std::weak_ptr<Server>;

        enum  UserResult { DONE, ASYNC };
        using Handler   = std::function<UserResult(SharedPtr, const Request&)>;

    private:
        Tcp::Socket  m_Socket;
        Util::EPoll* m_EPoll;
        Parser       m_Parser;
        Handler      m_Handler;

        const size_t BUFFER_SIZE = 4096;
        std::atomic_bool m_Error{false};

        Threads::SafeQueue<Request> m_Incoming;
        std::atomic_bool m_Busy{false};

        void process_request()
        {
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
            }
        }

        std::string m_WriteOut; // used only from network thread

        bool write_out()
        {
            if (m_WriteOut.empty())
                return true;
            ssize_t sSize = m_Socket.write(m_WriteOut.data(), m_WriteOut.size());
            if (sSize > 0)
                m_WriteOut.erase(0, sSize);
            if (m_WriteOut.empty())
                m_WriteOut.shrink_to_fit();
            return sSize >= 0;
        }

        bool write_int(const std::string& aData)
        {
            bool sIdle = m_WriteOut.empty();
            m_WriteOut.append(aData);
            if (sIdle)
                return write_out();
            return true;
        }

    public:

        Server(Tcp::Socket&& aSocket, Util::EPoll* aEPoll, Handler aHandler)
        : m_Socket(std::move(aSocket))
        , m_EPoll(aEPoll)
        , m_Parser([this](Request& a){ m_Incoming.insert(a); })
        , m_Handler(aHandler)
        { }

        bool is_error() const { return m_Error; }
        int get_fd() const { return m_Socket.get_fd(); }

        // call from Handler to write response
        void write(const std::string& aData)
        {
            if (m_Error)
                return;

            m_EPoll->post([p = shared_from_this(), aData](Util::EPoll* ptr) {
                if (!p->write_int(aData))
                {
                    p->on_error(p->get_fd());
                    ptr->erase(p->get_fd());
                }
            });
        }

        // called if async request processed
        void notify()
        {
            m_EPoll->post([p = shared_from_this()](Util::EPoll* ptr) {
                p->process_request();
            });
        }

        virtual Result on_read(int)
        {
            void* sBuffer = alloca(BUFFER_SIZE);
            size_t sSize = m_Socket.read(sBuffer, BUFFER_SIZE);
            if (sSize == 0)
                return CLOSE;
            size_t sUsed = m_Parser.consume((char*)sBuffer, sSize);
            if (sUsed < sSize)
                return CLOSE;
            if (!m_Busy and !m_Incoming.idle())
                process_request();
            return sSize < BUFFER_SIZE ? OK : RETRY;
        }
        virtual Result on_write(int)
        {
            return write_out() ? OK : CLOSE;
        }
        virtual void on_error(int) { m_Error = true; }
        virtual ~Server() {}
    };

    inline auto Make(Util::EPoll* aEPoll, Server::Handler aHandler)
    {
        return [aEPoll, aHandler](Tcp::Socket&& aSocket)
        {
            return std::make_shared<Server>(std::move(aSocket), aEPoll, aHandler);
        };
    }
} // namespace httpd