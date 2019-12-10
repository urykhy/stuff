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

        struct Request
        {
            uint64_t serial = 0;
            std::string body;
        };

    private:
        const uint64_t RECONNECT_MS = 1000;
        const uint32_t CONNECT_MS   = 10;

        ba::io_context::strand m_Strand;
        ba::deadline_timer     m_Timer;
        tcp::socket            m_Socket;
        const tcp::endpoint    m_Addr;
        const unsigned         m_Space;     // tnt space id
        ba::coroutine          m_Writer;
        ba::coroutine          m_Reader;
        std::list<Message>     m_Queue;     // write out queue
        State                  m_State;
        uint64_t               m_LastTry = 0;

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

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, unsigned aSpace)
        : m_Strand(aLoop)
        , m_Timer(aLoop)
        , m_Socket(aLoop)
        , m_Addr(aAddr)
        , m_Space(aSpace)
        { }

        void start()
        {
            m_Strand.get_io_service().post(resume_reader());
        }

        bool is_alive() { return m_State.is_alive(); }

        template<class K>
        Request formatSelect(const IndexSpec& aIndex, const K& aKey)
        {
            const uint64_t sSerial = m_Serial++;
            MsgPack::binary sBuffer;
            MsgPack::omemstream sStream(sBuffer);
            formatHeader(sStream, CODE_SELECT, sSerial);
            formatSelectBody(sStream, m_Space, aIndex);
            T::formatKey(sStream, aKey);
            return Request{sSerial, sBuffer};
        }

        Request formatInsert(const T& aData)
        {
            const uint64_t sSerial = m_Serial++;
            MsgPack::binary sBuffer;
            MsgPack::omemstream sStream(sBuffer);
            formatHeader(sStream, CODE_INSERT, sSerial);
            formatInsertBody(sStream, m_Space, aData);
            return Request{sSerial, sBuffer};
        }

        template<class K>
        Request formatDelete(const IndexSpec& aIndex, const K& aKey)
        {
            const uint64_t sSerial = m_Serial++;
            MsgPack::binary sBuffer;
            MsgPack::omemstream sStream(sBuffer);
            formatHeader(sStream, CODE_DELETE, sSerial);
            formatDeleteBody(sStream, m_Space, aIndex);
            T::formatKey(sStream, aKey);
            return Request{sSerial, sBuffer};
        }

        void call(const Request& aRequest, Handler&& aHandler, unsigned aTimeoutMS = 10)
        {
            post([this, p=this->shared_from_this(), aRequest, aHandler = std::move(aHandler), aTimeoutMS] () mutable
            {
                const bool sWriteOut = m_Queue.empty();
                if (!is_alive())
                {
                    auto ec = boost::system::error_code(boost::system::errc::not_connected, boost::system::system_category());
                    Promise sPromise;
                    sPromise.set_exception(std::make_exception_ptr(NetworkError(ec)));
                    aHandler(sPromise.get_future());
                    return;
                }

                m_Waiter.insert(aRequest.serial, aTimeoutMS, [p, aHandler = std::move(aHandler)](ReplyWaiter::Future&& aString){
                    p->callback(aHandler, std::move(aString));
                });
                m_Queue.emplace_back(Message(aRequest.body));
                if (sWriteOut) {
                    m_Writer = {};
                    writer();
                }
            });
        }

        void stop()
        {
            m_State.close();
            m_State.stop();
            // close socket from asio thread, to avoid a race
            post([p=this->shared_from_this()]() { p->close_socket(); });
        }

        // wrap any operation with this socket
        template<class X> auto wrap(X&& x) -> decltype(m_Strand.wrap(std::move(x))) { return m_Strand.wrap(std::move(x)); }
        template<class X> void post(X&& x) { m_Strand.post(std::move(x)); }

        boost::asio::io_service& io_service() { return m_Strand.get_io_service(); }

    private:

        void close_socket()
        {
            if (!m_Socket.is_open())
                return;
            boost::system::error_code ec;   // ignore shutdown error
            m_Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            m_Socket.close();
        }

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
            return wrap([p=this->shared_from_this()](boost::system::error_code ec = boost::system::error_code(), size_t size = 0){ p->writer(ec); });
        }

        void reader(boost::system::error_code ec = boost::system::error_code())
        {
            if (!ec and m_State.state() == TIMEOUT)
            {   // timer already closed socket
                ec = boost::system::error_code(boost::system::errc::operation_canceled, boost::system::system_category());
            }
            if (ec)
            {
                // if async_connect timed out - replace error message
                if (m_State.is_running() and
                    m_State.state() == CONNECTING
                    and ec == boost::system::error_code(boost::system::errc::operation_canceled, boost::system::system_category()))
                {
                    ec = boost::system::errc::make_error_code(boost::system::errc::timed_out);
                }
                BOOST_TEST_MESSAGE("callback called with error " << ec.message());
                on_error(ec);
                return;
            }

            reenter (&m_Reader)
            {
                m_State.connecting();
                m_Timer.expires_from_now(boost::posix_time::millisec(CONNECT_MS));
                m_Timer.async_wait(wrap([p=this->shared_from_this()](boost::system::error_code ec) {
                    if (!ec and p->m_State.state() == CONNECTING)
                    {
                        p->m_Socket.close();
                        p->m_State.timeout();
                    }
                }));
                yield m_Socket.async_connect(m_Addr, resume_reader());

                // auth. just read a greetings
                m_State.auth();
                yield async_read(m_Socket
                               , boost::asio::buffer(&m_Greetings, sizeof(m_Greetings))
                               , resume_reader());
                BOOST_TEST_MESSAGE("greeting from " << boost::string_ref(m_Greetings.version, 64));
                on_established();

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
            return wrap([p=this->shared_from_this()](boost::system::error_code ec = boost::system::error_code(), size_t size = 0){ p->reader(ec); });
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
                on_error(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
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

        // called once connection established
        void on_established()
        {
            m_Timer.cancel();       // stop connect timer
            m_State.established();  // switch state
            m_Waiter.reset();       // drop last error
            set_timer();            // fire timeout-check timer
        }

        // called on network/protocol error
        void on_error(boost::system::error_code ec)
        {
            m_Timer.cancel();                       // stop timer
            m_LastTry = Time::get_time().to_ms();   // save disconnect moment
            m_State.set_error();                    // set error state
            set_timer();                            // setup 1ms timer
            close_socket();                         // close socket

            std::exception_ptr sPtr = nullptr;
            // FIXME: pass real msgpack error ?
            if (ec == boost::system::errc::protocol_error)
                sPtr = std::make_exception_ptr(ProtocolError("fail to parse response"));
            else
                sPtr = std::make_exception_ptr(NetworkError(ec));

            m_Waiter.flush(sPtr);                   // drop all pending requests with error
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
            check_reconnect();
            m_Waiter.on_timer(); // report timeout on slow calls
            set_timer();
        }

        void check_reconnect()
        {
            const uint64_t sNow = Time::get_time().to_ms();

            if (m_State.is_running() and !m_State.is_connected() and m_LastTry + RECONNECT_MS < sNow)
            {   // reset coroutines and start connecting
                BOOST_TEST_MESSAGE("reconnecting");
                m_Writer = {};
                m_Reader = {};
                m_LastTry = sNow;
                start();
            }
        }
    };
} // namespace tnt17