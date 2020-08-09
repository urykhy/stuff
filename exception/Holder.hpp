#pragma once

#include <mutex>
#include <stdexcept>

namespace Exception {
    class Holder
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::exception_ptr m_Exception;

    public:
        void collect()
        {
            if (Lock lk(m_Mutex); !m_Exception)
                m_Exception = std::current_exception();
        }

        void raise() const
        {
            if (Lock lk(m_Mutex); m_Exception)
                std::rethrow_exception(m_Exception);
        }

        operator bool() const
        {
            Lock lk(m_Mutex);
            return (bool)m_Exception;
        }
    };
} // namespace Exception