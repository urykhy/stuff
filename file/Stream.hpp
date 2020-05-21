#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <exception/Error.hpp>

namespace File::Stream
{
    class Reader : public std::streambuf
    {
        std::string m_Name;
        std::string m_Buffer;
        int m_FD = -1;
    protected:

        int underflow() override
        {
            if (m_FD == -1)
                return traits_type::eof();

            if (gptr() < egptr())
                return *gptr();

            ssize_t sSize = ::read(m_FD, m_Buffer.data(), m_Buffer.size());
            if (sSize < 0)
                throw Exception::ErrnoError("fail to read " + m_Name);
            if (sSize > 0)
            {
                setg(m_Buffer.data(), m_Buffer.data(), m_Buffer.data() + sSize);
                return *gptr();
            }
            return traits_type::eof();
        }
    public:

        Reader(size_t aSize = 64 * 1024)
        : m_Buffer(aSize, 'x')
        {}

        ~Reader() { close(); }

        bool open(const std::string& aName)
        {
            close();
            m_FD = ::open(aName.c_str(), O_RDONLY);
            if (m_FD == -1)
                throw Exception::ErrnoError("fail to open " + aName);
            m_Name = aName;
            setg(m_Buffer.data(), m_Buffer.data() + m_Buffer.size(), m_Buffer.data() + m_Buffer.size());
            return true;
        }

        void close()
        {
            if (m_FD > -1)
            {
                ::close(m_FD);
                m_FD = -1;
                m_Name.clear();
            }
        }
    };

    class Writer : public std::streambuf
    {
        std::string m_Name;
        std::string m_Buffer;
        int m_FD = -1;

    protected:

        int overflow(int c) override
        {
            if (m_FD == -1)
                return traits_type::eof();

            if (c != traits_type::eof())
            {   // using extra space in buffer to store this character
                *pptr() = c;
                pbump(1);
            }

            return sync() == traits_type::eof() ? traits_type::eof() : traits_type::not_eof(c);
        }

        int sync() override
        {
            if (pptr() == pbase())
                return 0;

            ptrdiff_t sSize = pptr() - pbase();

            ssize_t sR = ::write(m_FD, pbase(), sSize);
            if (sR < 0)
                throw Exception::ErrnoError("fail to write " + m_Name);
            if (sR == sSize)
            {
                pbump(-sSize);
                return 0;
            }
            throw std::runtime_error("partial write to " + m_Name);
        }

    public:
        Writer(size_t aSize = 64)
        : m_Buffer(aSize, 'x')
        {}

        ~Writer() { close(); }

        bool open(const std::string& aName, int aFlags = 0) // O_APPEND | O_TRUNC
        {
            close();
            m_FD = ::open(aName.c_str(), O_WRONLY | O_CREAT | aFlags, 0644);
            if (m_FD == -1)
                throw Exception::ErrnoError("fail to open " + aName);
            m_Name = aName;
            setp(m_Buffer.data(), m_Buffer.data() + m_Buffer.size() - 1); // extra space for 1 character, to use on overflow
            return true;
        }

        void close()
        {
            if (m_FD > -1)
            {
                ::close(m_FD);
                m_FD = -1;
                m_Name.clear();
            }
        }
    };
}