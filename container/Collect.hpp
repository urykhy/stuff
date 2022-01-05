#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace Container {

    // collect data to vector, call handler once vector full.
    template <class T>
    struct Collect
    {
        using Handler = std::function<void(std::vector<T>&)>;

    private:
        mutable std::mutex                   m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;

        const size_t   m_Max;
        std::vector<T> m_Buffer;
        Handler        m_Handler;

        void flush_i()
        {
            if (m_Buffer.empty())
                return;
            m_Handler(m_Buffer);
            m_Buffer = {};
            m_Buffer.reserve(m_Max);
        }

    public:
        Collect(size_t aMax, Handler&& aHandler)
        : m_Max(aMax)
        , m_Handler(std::move(aHandler))
        {
            m_Buffer.reserve(m_Max);
        }

        void insert(T&& aData)
        {
            Lock lk(m_Mutex);
            m_Buffer.push_back(std::move(aData));
            if (m_Buffer.size() == m_Max)
                flush_i();
        }

        void flush()
        {
            Lock lk(m_Mutex);
            flush_i();
        }

        size_t size() const
        {
            Lock lk(m_Mutex);
            return m_Buffer.size();
        }

        bool idle() const
        {
            Lock lk(m_Mutex);
            return m_Buffer.empty();
        }
    };
} // namespace Container
