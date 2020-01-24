#pragma once

#include <string>
#include "Parser.hpp"
#include <networking/EPoll.hpp>
#include <networking/TcpSocket.hpp>
#include <networking/TcpListener.hpp>

namespace httpd
{
    struct Server : public Util::EPoll::HandlerFace, std::enable_shared_from_this<Server>
    {
        using WeakPtr   = std::weak_ptr<Server>;
        using Handler = std::function<void(WeakPtr, Request&)>;

    private:
        Tcp::Socket m_Socket;
        Handler     m_Handler;
        const size_t BUFFER_SIZE = 4096;
        std::string m_Buffer;
        std::unique_ptr<Parser> m_Parser;

    public:
        Server(Tcp::Socket&& aSocket, Handler aHandler)
        : m_Socket(std::move(aSocket))
        , m_Handler(aHandler)
        , m_Buffer(BUFFER_SIZE, ' ')
        { }

        void start()
        {
            auto sWeak = WeakPtr(shared_from_this());
            m_Parser = std::make_unique<Parser>([this, sWeak](Request& sRequest){
                this->m_Handler(sWeak, sRequest);
            });
        }

        // call from Handler to write response
        void write(std::string& aData)
        {
            size_t sSize = m_Socket.write(aData.data(), aData.size());
            if (sSize < aData.size())
                throw "fail to write out";
        }
        // add is_alive to check connection state
        // ignore write if connection dead ?
        // report write errors
        // write out queue
        // post to transport write requests to avoid locking ?

        virtual Result on_read(int)
        {
            size_t sSize = m_Socket.read(m_Buffer.data(), m_Buffer.size());
            if (sSize == 0)
                return CLOSE;
            m_Parser->consume(m_Buffer.data(), sSize);
            return sSize < m_Buffer.size() ? OK : RETRY;
        }
        virtual Result on_write(int) {
            return OK;
        }
        virtual void on_error(int) { }
        virtual ~Server() {}
    };

    inline auto Make(Server::Handler aHandler)
    {
        return [aHandler](Tcp::Socket&& aSocket)
        {
            auto s = std::make_shared<Server>(std::move(aSocket), aHandler);
            s->start();
            return s;
        };
    }
} // namespace httpd