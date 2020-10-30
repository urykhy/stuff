#pragma once

#include <sys/types.h>

#include <memory>

#include <boost/core/noncopyable.hpp>

namespace File {

    constexpr unsigned DEFAULT_BUFFER_SIZE = 128 * 1024;

    struct IReader : public boost::noncopyable
    {
        virtual size_t read(void* aPtr, size_t aSize) = 0;
        virtual bool   eof()                          = 0;
        virtual void   close()                        = 0;

        virtual ~IReader() noexcept(false) {}
    };

    // read not less than requested
    struct IExactReader : public IReader
    {
        virtual ~IExactReader() noexcept(false) {}
    };

    // mem reader can `read` substring
    struct IMemReader : public IExactReader
    {
        virtual std::string_view substring(size_t aSize) = 0;
        virtual ~IMemReader() noexcept(false) {}
    };

    struct IWriter : public boost::noncopyable
    {
        virtual void write(const void* aPtr, size_t aSize) = 0;
        virtual void flush()                               = 0;
        virtual void sync()                                = 0;
        virtual void close()                               = 0;

        virtual ~IWriter() noexcept(false) {}
    };

    struct Dumpable
    {
        virtual void dump(IWriter*)         = 0;
        virtual void restore(IExactReader*) = 0;
        virtual ~Dumpable(){};
    };
} // namespace File
