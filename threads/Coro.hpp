#pragma once

#include <concepts>
#include <coroutine>
#include <exception>

namespace Threads::Coro {

    // copy paste from https://www.scs.stanford.edu/~dm/blog/c++-coroutines.html
    struct Return
    {
        struct promise_type
        {
            std::exception_ptr exception_;

            Return get_return_object()
            {
                return {
                    .h_ = std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            std::suspend_never  initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void                unhandled_exception() { exception_ = std::current_exception(); }
            void                return_void() {}
        };

        std::coroutine_handle<promise_type> h_;
        operator std::coroutine_handle<promise_type>() const { return h_; }
        operator std::coroutine_handle<>() const { return h_; }
    };

    using Handle = std::coroutine_handle<Return::promise_type>;

} // namespace Threads::Coro