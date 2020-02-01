#pragma once

#include "Server.hpp"

#include <threads/SafeQueue.hpp>
#include <threads/Group.hpp>

namespace httpd
{
    struct Router
    {
        using Worker = Threads::SafeQueueThread<std::function<void()>>;
        using UserResult = Server::UserResult;

        struct Location
        {
            std::string method;
            std::string prefix;
            bool async = true;
        };
        using Handler = std::function<UserResult(Server::SharedPtr, const Request&)>;

    private:
        Worker m_Worker;
        std::list<std::pair<Location, Handler>> m_Locations;

        bool match(const Location& aLoc, const Request& aReq)
        {
            if (aReq.m_Method != aLoc.method)
                return false;
            if (0 != aReq.m_Url.compare(0, aLoc.prefix.size(), aLoc.prefix))
                return false;
            return true;
        }

        UserResult process(Server::SharedPtr aServer, const Request& aRequest, const Handler& aHandler, bool aAsync)
        {
            if (aAsync)
            {
                // use WeakPtr to pass task via queue, so if connection closes
                // it will be destroyed and task dropped
                Server::WeakPtr sWeak = aServer;
                m_Worker.insert([sWeak, aRequest, aHandler]()
                {
                    auto sServer = sWeak.lock();
                    if (sServer) {
                        auto rc = aHandler(sServer, aRequest);
                        if (rc == UserResult::DONE)
                            sServer->notify();
                        // if handler return ASYNC - user must call notify() once request done
                    }
                });
                return UserResult::ASYNC;
            }
            return aHandler(aServer, aRequest);
        }

    public:
        Router()
        : m_Worker([](std::function<void()>& aCall) { aCall(); })
        { }

        void start(Threads::Group& aGroup) { m_Worker.start(aGroup); }
        void insert(const Location& aLoc, const Handler aHandler) { m_Locations.push_back({aLoc, aHandler}); }

        UserResult operator()(Server::SharedPtr aServer, const Request& aRequest)
        {
            for (auto& [sLoc, sHandler]: m_Locations)
                if (match(sLoc, aRequest))
                    return process(aServer, aRequest, sHandler, sLoc.async);

            const std::string sNotFound = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            aServer->write(sNotFound);  // FIXME: write_int, since we in the same thread
            return UserResult::DONE;
        }
    };

} // namespace httpd