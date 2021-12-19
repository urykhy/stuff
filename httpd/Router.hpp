#pragma once

#include <threads/Group.hpp>
#include <threads/SafeQueue.hpp>

#include "Connection.hpp"

namespace httpd {
    struct Router
    {
        using Worker     = Threads::SafeQueueThread<std::function<void()>>;
        using UserResult = Connection::UserResult;

        struct Location
        {
            std::string prefix;
            bool        async = true;
        };
        using Handler = std::function<UserResult(Connection::SharedPtr, const Request&)>;

    private:
        Worker                                  m_Worker;
        std::list<std::pair<Location, Handler>> m_Locations;

        bool match(const Location& aLoc, const Request& aReq) const
        {
            return 0 == aReq.url.compare(0, aLoc.prefix.size(), aLoc.prefix);
        }

        UserResult process(Connection::SharedPtr aConnection, const Request& aRequest, const Handler& aHandler, bool aAsync)
        {
            if (aAsync) {
                // use WeakPtr to pass task via queue, so if connection closes
                // it will be destroyed and task dropped
                Connection::WeakPtr sWeak = aConnection;
                m_Worker.insert([sWeak, aRequest, aHandler]() {
                    auto sConnection = sWeak.lock();
                    if (sConnection)
                        sConnection->notify(aHandler(sConnection, aRequest));
                    // if handler return ASYNC - user must call notify() with proper value by self
                });
                // return async state to Connection
                return UserResult::ASYNC;
            }
            return aHandler(aConnection, aRequest);
        }

    public:
        Router()
        : m_Worker([](std::function<void()>& aCall) { aCall(); })
        {}

        void start(Threads::Group& aGroup) { m_Worker.start(aGroup); }
        void insert_sync(const std::string& aLoc, const Handler aHandler) { m_Locations.push_back({Location{aLoc, false}, aHandler}); }
        void insert(const std::string& aLoc, const Handler aHandler) { m_Locations.push_back({Location{aLoc, true}, aHandler}); }

        UserResult operator()(Connection::SharedPtr aConnection, const Request& aRequest)
        {
            for (auto& [sLoc, sHandler] : m_Locations)
                if (match(sLoc, aRequest))
                    return process(aConnection, aRequest, sHandler, sLoc.async);

            const std::string sNotFound = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            aConnection->write(sNotFound);
            return UserResult::DONE;
        }
    };

} // namespace httpd