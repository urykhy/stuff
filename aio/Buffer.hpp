#pragma once

#include <malloc.h>
#include <exception/Error.hpp>

namespace AIO
{
    class Buffer
    {
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        unsigned char* m_Buffer = 0;
        size_t m_Size = 0;
    public:

        using Error = Exception::ErrnoError;

        Buffer(size_t aSize) : m_Size(aSize)
        {
            m_Buffer = (unsigned char*)memalign(4096, aSize);
            if (!m_Buffer)
                throw Error("allocation error");
            memset(m_Buffer, 0 , aSize);
        }
        ~Buffer() { free(m_Buffer); }

        void*  data() const { return m_Buffer; }
        size_t size() const { return m_Size; }
    };
}