#pragma once

#include <sys/types.h>

#include <memory>

namespace File {

    constexpr unsigned DEFAULT_BUFFER_SIZE = 128 * 1024;

    struct IReader
    {
        virtual size_t read(char* aPtr, size_t aSize) = 0;
        virtual bool   eof()                          = 0;
        virtual void   close()                        = 0;
        virtual ~IReader() noexcept(false) {}
    };

    struct IWriter
    {
        virtual void write(const char* aPtr, size_t aSize) = 0;
        virtual void flush()                               = 0;
        virtual void sync()                                = 0;
        virtual void close()                               = 0;
        virtual ~IWriter() noexcept(false) {}
    };
} // namespace File
