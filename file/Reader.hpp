#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Interface.hpp"

#include <archive/Interface.hpp>
#include <exception/Error.hpp>
#include <unsorted/Raii.hpp>
#include <unsorted/Unwind.hpp>

namespace File {

    class FileReader : public IReader
    {
        int          m_FD  = -1;
        bool         m_EOF = false;
        Util::Unwind m_Unwind;

    public:
        FileReader(const std::string& aName)
        {
            Util::Raii sGuard([this]() {
                close();
            });
            m_FD = ::open(aName.c_str(), O_RDONLY);
            if (m_FD == -1)
                throw Exception::ErrnoError("FileReader: fail to open: " + aName);

            struct stat sStat;
            memset(&sStat, 0, sizeof(sStat));
            auto sRC = fstat(m_FD, &sStat);
            if (sRC != 0)
                throw Exception::ErrnoError("FileReader: fail to stat: " + aName);

            sRC = posix_fadvise(m_FD, 0, sStat.st_size, POSIX_FADV_SEQUENTIAL);
            if (sRC != 0)
                throw Exception::ErrnoError("FileReader: fail to fadvise: " + aName);
            sGuard.dismiss();
        }
        size_t read(void* aPtr, size_t aSize) override
        {
            ssize_t sRC = ::read(m_FD, aPtr, aSize);
            if (sRC == -1)
                throw Exception::ErrnoError("FileReader: fail to read");
            if (sRC == 0)
                m_EOF = true;
            return sRC;
        }
        bool eof() override
        {
            return m_EOF;
        }
        void close() override
        {
            if (m_FD == -1)
                return;
            int sRC = ::close(m_FD);
            m_FD    = -1;
            if (sRC == -1)
                throw Exception::ErrnoError("FileReader: fail to close");
        }
        ~FileReader() noexcept(false)
        {
            try {
                close();
            } catch (...) {
                if (!m_Unwind())
                    throw;
            }
        }
    };

    class BufReader : public IReader
    {
    protected:
        IReader*    m_Reader;
        std::string m_Buffer;
        size_t      m_Pos = 0;
        size_t      m_End = 0;
        //
        // m_Buffer.data ............ m_ReadPos .......... m_End ...... m_Buffer.end
        //             <already readed>       <data to read>   < unused >

        char*  ptr() { return &m_Buffer[m_Pos]; }
        size_t avail() const { return m_End - m_Pos; }

        virtual void refill()
        {
            m_End = m_Reader->read(&m_Buffer[0], m_Buffer.size());
            m_Pos = 0;
        }

    public:
        BufReader(IReader* aReader, size_t aLen = DEFAULT_BUFFER_SIZE)
        : m_Reader(aReader)
        , m_Buffer(aLen, ' ')
        {}

        size_t read(void* aPtr, size_t aLen) override
        {
            char* sPtr = (char*)aPtr;
            if (eof())
                return 0;

            size_t sUsed = 0;
            while (sUsed < aLen) {
                if (avail() == 0)
                    refill();
                auto sMinSize = std::min(aLen - sUsed, avail());
                memcpy(sPtr + sUsed, ptr(), sMinSize);
                m_Pos += sMinSize;
                sUsed += sMinSize;
                if (eof())
                    break;
            }
            return sUsed;
        }

        bool eof() override { return avail() == 0 and m_Reader->eof(); }
        void close() override { m_Reader->close(); }
    };

    class FilterReader : public BufReader
    {
        Archive::IFilter* m_Filter;
        bool              m_Eof = false;

        void refill() override
        {
            m_End = m_Reader->read(&m_Buffer[0], m_Buffer.size());
            m_Pos = 0;
        }

    public:
        FilterReader(IReader* aReader, Archive::IFilter* aFilter, size_t aLen = DEFAULT_BUFFER_SIZE)
        : BufReader(aReader, aLen)
        , m_Filter(aFilter)
        {}

        size_t read(void* aPtr, size_t aLen) override
        {
            char* sPtr = (char*)aPtr;
            if (m_Eof)
                return 0;

            size_t sUsed = 0;
            while (sUsed < aLen) {
                if (avail() == 0)
                    refill();

                size_t sAvailSrc = avail();
                if (sAvailSrc > 0) {
                    auto sInfo = m_Filter->filter(ptr(), avail(), sPtr + sUsed, aLen - sUsed);
                    m_Pos += sInfo.usedSrc;
                    sUsed += sInfo.usedDst;
                } else if (m_Reader->eof()) {
                    auto sInfo = m_Filter->finish(sPtr + sUsed, aLen - sUsed);
                    sUsed += sInfo.usedDst;
                    if (sInfo.done) {
                        m_Eof = true;
                        break;
                    }
                }
            }
            return sUsed;
        }

        bool eof() override { return m_Eof; }
        void close() override { m_Reader->close(); }
    };

    class ExactReader : public IExactReader
    {
        IReader* m_Parent;
        bool     m_Unget = false;
        uint8_t  m_Last  = 0;

    public:
        ExactReader(IReader* aParent)
        : m_Parent(aParent)
        {}

        // return last character into read queue
        void unget() override
        {
            if (m_Unget)
                throw std::runtime_error("unget already called");
            m_Unget = true;
        }

        size_t read(void* aPtr, size_t aSize) override
        {
            if (m_Unget) {
                *reinterpret_cast<uint8_t*>(aPtr) = m_Last;

                m_Unget = false;
                aSize--;
                if (aSize == 0)
                    return 1;
                aPtr   = (reinterpret_cast<uint8_t*>(aPtr)) + 1;
                m_Last = 0;
            }

            size_t sSize = m_Parent->read(aPtr, aSize);
            if (sSize < aSize)
                throw std::invalid_argument("ExactReader: eof");

            m_Last = (reinterpret_cast<uint8_t*>(aPtr))[sSize - 1];

            return sSize;
        }
        bool eof() override { return m_Parent->eof(); }
        void close() override { m_Parent->close(); }
    };

} // namespace File