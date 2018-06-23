
#pragma once
#include <exception>
#include <TaggedException.hpp>
#include <TimeMeter.hpp>
#include <future>

namespace Util {

    struct _base;
    struct _remote;
    struct _timeout;
    struct _dead;
    typedef TaggedException<_base>    Exception;
    typedef TaggedException<_remote,  Exception> Remote;
    typedef TaggedException<_timeout, Exception> Timeout;
    typedef TaggedException<_dead,    Exception> Dead;

    template<class T>
    class Future
    {
        typedef std::unique_lock<std::mutex> Lock;
        mutable std::mutex mutex;
        std::condition_variable cond;
        T result;
        std::exception_ptr error;
        bool done;

        const T& get_i()
        {
            if (!done) throw Exception("not ready");
            if (!error) {
                return result;
            }
            std::rethrow_exception(error);
        }
    public:
        using value_type = T;
        Future() : done(false) {}

        Future(Future&& x) : result(std::move(x.result)), error(std::move(x.error)), done(x.done) { }
        Future& operator=(Future&& x) {
            Lock lk(mutex);
            result = std::move(x.result);
            error = std::move(x.error);
            done = x.done;
            x.done = false;
            return *this;
        }

        void reset()
        {
            Lock lk(mutex);
            done = false;
            result = T();
            error = std::exception_ptr{};
        }

        void set_value(const T& t)
        {
            Lock lk(mutex);
            assert (!done);
            result = t;
            done = true;
            cond.notify_one();
        }

        void set_value(T&& t)
        {
            Lock lk(mutex);
            assert (!done);
            result = t;
            done = true;
            cond.notify_one();
        }

        void set_error(std::exception_ptr e)
        {
            Lock lk(mutex);
            assert (!done);
            error = e;
            done = true;
            cond.notify_one();
        }

        const T& get(const TimeoutHelper& th)
        {
            return get(std::chrono::milliseconds(th.rest()));
        }

        // relative timeout
        // std::chrono::seconds(1)
        // std::chrono::milliseconds(30)
        template<class O>
        const T& get(const O& timeout)
        {
            std::cv_status res = std::cv_status::no_timeout;
            Lock lk(mutex);
            if (!done) {
                res = cond.wait_for(lk, timeout);
            }
            if (res == std::cv_status::no_timeout)
            {
                return get_i();
            }
            throw Timeout("Timeout");
        }

        const T& get()
        {
            Lock lk(mutex);
            return get_i();
        }
    };

} // namespace Util

