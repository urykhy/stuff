#pragma once

#include <optional>

#include "Client.hpp"

namespace tnt17
{
    // no locks since all access from asio thread with strand
    // FIXME: add KEY template parameter as well
    // FIXME: add IndexSpec
    template<class T>
    struct FetchQueue
    {
        std::list<unsigned> requests;
        std::list<T>        responses;

        bool empty() const { return requests.empty(); }

        std::optional<unsigned> get()
        {
            std::optional<unsigned> sResult;
            if (!requests.empty())
            {
                sResult = requests.front();
                requests.pop_front();
            }
            return sResult;
        }
        void push_back(T&& t)
        {
            responses.push_back(std::move(t));
        }
    };

    template<class T>
    struct Fetch : public std::enable_shared_from_this<Fetch<T>>
    {
        using XClient    = Client<T>;
        using XClientPtr = std::shared_ptr<XClient>;
    private:

        XClientPtr      m_Client;
        FetchQueue<T>&  m_Queue;
        std::unique_ptr<ba::coroutine> m_Coro;
        ba::deadline_timer        m_Timer;
        State                     m_State;
        std::optional<unsigned>   m_Key;      // current key
        typename XClient::Request m_Request;  // current request
        bool                      m_Queued{false};
    public:

        Fetch(XClientPtr aClient, FetchQueue<T>& aQueue)
        : m_Client(aClient)
        , m_Queue(aQueue)
        , m_Coro(std::make_unique<ba::coroutine>())
        , m_Timer(aClient->io_service())
        {}

        void start() { m_State.established(); m_Client->post(resume()); }
        void stop()  { m_State.stop(); }
        bool is_running() const { return m_State.is_running(); }
        bool is_done()    const { return m_State.state() == IDLE; }
    private:

#include <boost/asio/yield.hpp>
        void operator()(boost::system::error_code ec = boost::system::error_code())
        {
            if (!is_running()) return;
            if (ec) m_Coro = std::make_unique<ba::coroutine>(); // if error - restart coroutine

            reenter (m_Coro.get())
            {
                while(true)
                {
                    yield ensure_connected();

                    if (!m_Key) // get next key
                        m_Key = m_Queue.get();

                    if (!m_Key) // no more keys
                    {
                        m_State.close();
                        return;
                    }

                    m_Request = m_Client->formatSelect(tnt17::IndexSpec{}.set_id(0), *m_Key);
                    m_Queued  = m_Client->call(m_Request.first, m_Request.second, [this, aHandler = resume()](typename XClient::Future&& aResult) mutable
                    {
                        try {
                            // FIXME: notify if really no data ?
                            auto sResult = aResult.get();
                            for (auto& x : sResult)
                                m_Queue.push_back(std::move(x));
                            aHandler();
                        } catch (const std::exception& e) {
                            BOOST_TEST_MESSAGE("got exception: " << e.what());
                            aHandler(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
                        }
                    });
                    if (!m_Queued)
                        continue;
                    yield;
                    BOOST_TEST_MESSAGE("key " << *m_Key << " fetched");
                    m_Key = {}; // key processed
                }
            }
        }
        auto resume()
        {
            return wrap([p=this->shared_from_this()](boost::system::error_code ec = boost::system::error_code(), size_t size = 0){ p->operator()(ec); });
        }
#include <boost/asio/unyield.hpp>

        // wrap any operation with this socket
        template<class X> auto wrap(X&& x) -> decltype(m_Client->wrap(std::move(x))) { return m_Client->wrap(std::move(x)); }
        template<class X> void post(X&& x) { m_Client->post(std::move(x)); }

        // periodic timer to check if connection established
        void set_timer()
        {
            const unsigned TIMEOUT_MS = 1; // check timeouts every 1ms
            if (!m_State.is_running())
                return;

            m_Timer.expires_from_now(boost::posix_time::millisec(TIMEOUT_MS));
            m_Timer.async_wait(wrap([p=this->shared_from_this()](auto error)
            {
                if (!error)
                    p->ensure_connected();
            }));
        }

        void ensure_connected()
        {
            if (!m_Client->is_alive())
                set_timer();
            else
                post(resume());
        }
    };
} // namespace tnt17