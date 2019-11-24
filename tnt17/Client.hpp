#pragma once

#include "Message.hpp"
#include "Protocol.hpp"
#include "ReplyWaiter.hpp"
#include "State.hpp"

namespace tnt17
{
    tcp::endpoint endpoint(const std::string& aAddr, uint16_t aPort)
    {
        return tcp::endpoint(ba::ip::address::from_string(aAddr), aPort);
    }

    // framed server and client
    template<class T>
    class Client : public std::enable_shared_from_this<Client<T>>
    {
    public:
        using Result = std::vector<T>;
        using Promise = std::promise<Result>;
        using Future  = std::future<Result>;
        using Handler = std::function<void(Future&&)>;

    private:
        ba::io_context::strand m_Strand;
        ba::deadline_timer     m_Timer;
        tcp::socket            m_Socket;
        const tcp::endpoint    m_Addr;
        const unsigned         m_Space;     // tnt space id
        ba::coroutine          m_Writer;
        ba::coroutine          m_Reader;
        std::list<Message>     m_Queue;     // write out queue
        State                  m_State;

        struct Greetings
        {
            char version[64];
            char salt[44];
            char dummy[20];
        } __attribute__((packed));
        Greetings m_Greetings;

        std::atomic<uint64_t>  m_Serial{1};
        ReplyWaiter            m_Waiter;

        typename Message::Header m_ReplyHeader;
        std::string              m_ReplyData;

        // wrap any operation with this socket
        template<class X> auto wrap(X&& x) -> decltype(m_Strand.wrap(std::move(x))) { return m_Strand.wrap(std::move(x)); }
        template<class X> void post(X&& x) { m_Strand.post(std::move(x)); }

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, unsigned aSpace)
        : m_Strand(aLoop)
        , m_Timer(aLoop)
        , m_Socket(aLoop)
        , m_Addr(aAddr)
        , m_Space(aSpace)
        { }

        void start() {
            reader();
        }

        bool is_alive() { return m_State.is_alive(); }

        template<class K>
        std::pair<uint64_t, std::string> formatSelect(const IndexSpec& aIndex, const K& aKey)
        {
            const uint64_t sSerial = m_Serial++;
            MsgPack::binary sBuffer;
            MsgPack::omemstream sStream(sBuffer);
            formatHeader(sStream, CODE_SELECT, sSerial);
            formatSelectBody(sStream, m_Space, aIndex);
            T::formatKey(sStream, aKey);
            return std::make_pair(sSerial, sBuffer);
        }

        bool call(size_t aSerial, const std::string& aRequest, Handler&& aHandler)
        {
            if (!is_alive())
                return false;

            post([this, p=this->shared_from_this(), aSerial, aRequest, aHandler = std::move(aHandler)] () mutable
            {
                const unsigned sTimeoutMs = 100;        // FIXME
                const bool sWriteOut = m_Queue.empty();
                m_Waiter.insert(aSerial, sTimeoutMs, [p, aHandler = std::move(aHandler)](ReplyWaiter::Future&& aString){
                    p->callback(aHandler, std::move(aString));
                });
                m_Queue.emplace_back(Message(aRequest));
                if (sWriteOut)
                    writer();
            });
            return true;
        }

        void stop()
        {
            m_State.close();
            m_State.stop();
            // close socket from asio thread, to avoid a race
            post([p=this->shared_from_this()]()
            {
                if (!p->m_Socket.is_open()) return;
                boost::system::error_code ec;   // ignore shutdown error
                p->m_Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                p->m_Socket.close();
            });
        }

    private:
#include <boost/asio/yield.hpp>
        void writer(boost::system::error_code ec = boost::system::error_code())
        {
            if (ec || !m_State.is_alive())
            {
                m_Queue.clear();
                return;
            }

            reenter (&m_Writer)
            {
                while (!m_Queue.empty())
                {
                    yield boost::asio::async_write(m_Socket, m_Queue.front().as_buffer(), resume_writer());
                    m_Queue.pop_front();
                }
            }
        }
        auto resume_writer() {
            return wrap([p=this->shared_from_this()](boost::system::error_code ec, size_t size = 0){ p->writer(ec); });
        }

        void reader(boost::system::error_code ec = boost::system::error_code())
        {
            if (ec)
            {
                m_State.set_error();
                callback(ec);
                return;
            }

            reenter (&m_Reader)
            {
                m_State.connecting();
                m_Timer.expires_from_now(boost::posix_time::millisec(100)); // 100 ms to connect
                m_Timer.async_wait(wrap([p=this->shared_from_this()](boost::system::error_code ec){
                    if (!ec) p->m_Socket.close();
                }));
                yield m_Socket.async_connect(m_Addr, resume_reader());

                // auth. just read a greetings
                yield async_read(m_Socket
                               , boost::asio::buffer(&m_Greetings, sizeof(m_Greetings))
                               , resume_reader());
                BOOST_TEST_MESSAGE("greeting from " << boost::string_ref(m_Greetings.version, 64));
                m_Timer.cancel();
                m_State.established();
                set_timer();

                while (true)
                {
                    yield boost::asio::async_read(m_Socket, boost::asio::buffer(&m_ReplyHeader, sizeof(m_ReplyHeader)), resume_reader());
                    m_ReplyData.resize(m_ReplyHeader.decode());
                    yield boost::asio::async_read(m_Socket, boost::asio::buffer(m_ReplyData), resume_reader());
                    callback(m_ReplyData);
                }
            }
        }
        auto resume_reader() {
            return wrap([p=this->shared_from_this()](boost::system::error_code ec, size_t size = 0){ p->reader(ec); });
        }
#include <boost/asio/unyield.hpp>

        // stage1 callback. parse server reply
        void callback(std::string& aReply)
        {
            Header sHeader;
            Reply  sReply;
            imemstream sStream(aReply);

            try {
                sHeader.parse(sStream);
                sReply.parse(sStream);
            } catch (const std::exception& e) {
                // severe error, drop connection
                callback(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
                return;
            }

            ReplyWaiter::Promise sPromise;
            if (sReply.ok)
            {
                const auto sRest = sStream.rest();
                std::string sRestStr(sRest.begin(), sRest.end());
                sPromise.set_value(sRestStr);
            } else {
                // normal error from server: just a bad client call. connection can be used
                sPromise.set_exception(std::make_exception_ptr(RemoteError(sReply.error)));
            }

            // jump to stage 2 callback
            m_Waiter.call(sHeader.sync, sPromise.get_future());
        }

        // stage 2 callback, from m_Waiter
        void callback(const Handler& aHandler, ReplyWaiter::Future&& aString)
        {
            Promise sPromise;
            std::string sString;

            try {
                sString = aString.get();
            }
            catch (...) {
                sPromise.set_exception(std::current_exception());
                aHandler(sPromise.get_future());
                return;
            };

            try {
                imemstream sStream(sString);
                const uint32_t sCount = MsgPack::read_array_size(sStream);
                Result sResult{sCount};
                for (auto& x : sResult)
                    x.parse(sStream);
                sPromise.set_value(sResult);
            } catch (const std::exception& e) {   // fail to parse response
                sPromise.set_exception(std::make_exception_ptr(ProtocolError(e.what())));
            }
            aHandler(sPromise.get_future());
        }

        // called on network/protocol error
        void callback(boost::system::error_code ec)
        {
            std::exception_ptr sPtr = nullptr;
            BOOST_TEST_MESSAGE("callback called with error " << ec.message());

            // FIXME: pass real msgpack error ?
            if (ec == boost::system::errc::protocol_error)
                sPtr = std::make_exception_ptr(ProtocolError("fail to parse response"));
            else
                sPtr = std::make_exception_ptr(NetworkError(ec));

            m_Waiter.flush(sPtr);    // drop all pending requests with error
        }
    private:

        // periodic timer to handle timeouts
        void set_timer()
        {
            const unsigned TIMEOUT_MS = 1; // check timeouts every 1ms
            if (!m_State.is_running() and m_Queue.empty())
            {
                BOOST_TEST_MESSAGE("timer stopped");
                return;
            }

            m_Timer.expires_from_now(boost::posix_time::millisec(TIMEOUT_MS));
            m_Timer.async_wait(wrap([p=this->shared_from_this()](auto error)
            {
                if (!error)
                    p->timeout_func();
            }));
        }

        void timeout_func()
        {
            m_Waiter.on_timer(); // report timeout on slow calls
            set_timer();
        }
    };
} // namespace tnt17