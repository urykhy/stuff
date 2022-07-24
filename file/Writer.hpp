#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Interface.hpp"

#include <archive/Interface.hpp>
#include <exception/Error.hpp>
#include <unsorted/Unwind.hpp>

namespace File {

    class FileWriter : public IWriter
    {
        int          m_FD = -1;
        Util::Unwind m_Unwind;

    public:
        explicit FileWriter(int aFD)
        : m_FD(aFD)
        {
        }
        FileWriter(const std::string& aName, int aFlags = 0) // O_APPEND | O_TRUNC; O_EXCL
        {
            m_FD = ::open(aName.c_str(), O_WRONLY | O_CREAT | aFlags, 0644);
            if (m_FD == -1)
                throw Exception::ErrnoError("FileWriter: fail to open: " + aName);
        }
        void write(const void* aPtr, size_t aSize) override
        {
            ssize_t sRC = ::write(m_FD, aPtr, aSize);
            if (sRC == -1)
                throw Exception::ErrnoError("FileWriter: fail to write");
            if ((size_t)sRC != aSize)
                throw Exception::ErrnoError("FileWriter: partial write");
        }
        void flush() override
        {
        }
        void sync() override
        {
            int sRC = fsync(m_FD);
            if (sRC == -1)
                throw Exception::ErrnoError("FileWriter: fail to sync");
        }
        void close() override
        {
            if (m_FD == -1)
                return;
            int sRC = ::close(m_FD);
            m_FD    = -1;
            if (sRC == -1)
                throw Exception::ErrnoError("FileWriter: fail to close");
        }
        virtual ~FileWriter() noexcept(false)
        {
            try {
                close();
            } catch (...) {
                if (!m_Unwind())
                    throw;
            }
        }
    };

    class BufWriter : public IWriter
    {
    protected:
        IWriter* m_Writer;

        std::string m_Buffer;
        size_t      m_End = 0;

        char*  ptr() { return &m_Buffer[m_End]; }
        size_t avail() const { return m_Buffer.size() - m_End; }

        void ensure(size_t aRequired)
        {
            if (aRequired > 0 and avail() < aRequired)
                m_Buffer.resize(aRequired + m_End);
        }

    public:
        BufWriter(IWriter* aWriter, size_t aLen = DEFAULT_BUFFER_SIZE)
        : m_Writer(aWriter)
        , m_Buffer(aLen, ' ')
        {
        }

        void write(const void* aPtr, size_t aSize) override
        {
            const char* sPtr  = (const char*)aPtr;
            size_t      sUsed = 0;
            while (sUsed < aSize) {
                size_t sMin = std::min(aSize - sUsed, avail());
                memcpy(ptr(), sPtr + sUsed, sMin);
                m_End += sMin;
                sUsed += sMin;
                if (m_End == m_Buffer.size()) {
                    flush();
                }
            }
        }
        void flush() override
        {
            if (m_End == 0)
                return;
            m_Writer->write(&m_Buffer[0], m_End);
            m_End = 0;
            m_Writer->flush();
        }
        void sync() override
        {
            flush();
            m_Writer->sync();
        };
        void close() override
        {
            flush();
            m_Writer->close();
        }
        virtual ~BufWriter()
        {
            close();
        }
    };

    class FilterWriter : public BufWriter
    {
        Archive::IFilter* m_Filter;

    public:
        FilterWriter(IWriter* aWriter, Archive::IFilter* aFilter, size_t aLen = DEFAULT_BUFFER_SIZE)
        : BufWriter(aWriter, aLen)
        , m_Filter(aFilter)
        {
        }
        void write(const void* aPtr, size_t aSize) override
        {
            const char* sPtr  = (const char*)aPtr;
            size_t      sUsed = 0;
            while (sUsed < aSize) {
                size_t sInputSize = std::min(DEFAULT_BUFFER_SIZE, aSize - sUsed);
                ensure(m_Filter->estimate(sInputSize));
                auto sInfo = m_Filter->filter(sPtr + sUsed, sInputSize, ptr(), avail());
                m_End += sInfo.usedDst;
                sUsed += sInfo.usedSrc;
                if (m_End == m_Buffer.size() or m_End >= DEFAULT_BUFFER_SIZE) {
                    flush();
                }
            }
        }
        void flush() override
        {
            while (true) {
                ensure(m_Filter->estimate(0));
                auto sInfo = m_Filter->finish(ptr(), avail());
                m_End += sInfo.usedDst;
                BufWriter::flush();
                if (sInfo.done)
                    break;
            }
        }
        virtual ~FilterWriter()
        {
            close();
        }
    };
} // namespace File
