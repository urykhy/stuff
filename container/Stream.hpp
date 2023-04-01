#pragma once

#include <string.h>

#include <cstdint>
#include <string>
#include <string_view>

#include <file/Interface.hpp>

namespace Container {
    using binary = std::string;

    class imemstream : public File::IMemReader
    {
        const std::string_view m_Data;
        size_t                 m_Offset = 0;

        void ensure(size_t aSize)
        {
            if (aSize + m_Offset > m_Data.size())
                throw EndOfBuffer();
        }

    public:
        struct EndOfBuffer : std::invalid_argument
        {
            EndOfBuffer()
            : std::invalid_argument("Premature end of buffer")
            {
            }
        };

        imemstream(const std::string_view& aBuffer)
        : m_Data(aBuffer)
        {
        }

        imemstream(const binary& aBuffer)
        : m_Data((const char*)&aBuffer[0], aBuffer.size())
        {
        }

        template <class T>
        void read(T& aData)
        {
            read(&aData, sizeof(aData));
        }

        void unget() override
        {
            if (m_Offset == 0)
                throw EndOfBuffer();
            m_Offset--;
        }

        void skip(size_t aSize)
        {
            ensure(aSize);
            m_Offset += aSize;
        }

        std::string_view rest() { return substring(m_Data.size() - m_Offset); }

        // File::IMemReader
        std::string_view substring(size_t aSize) override
        {
            ensure(aSize);
            std::string_view sResult = m_Data.substr(m_Offset, aSize);
            m_Offset += aSize;
            return sResult;
        }

        // File::IReader
        size_t read(void* aBuffer, size_t aSize) override
        {
            ensure(aSize);
            memcpy(aBuffer, m_Data.data() + m_Offset, aSize);
            m_Offset += aSize;
            return aSize;
        }

        bool eof() override { return m_Offset >= m_Data.size(); }
        void close() override {}

        // with user prvided offset
        size_t offset() const
        {
            return m_Offset;
        }
        std::string_view substr_at(size_t aOffset, size_t aSize) const
        {
            if (aSize + aOffset > m_Data.size())
                throw EndOfBuffer();
            return m_Data.substr(aOffset, aSize);
        }
    };

    class omemstream : public File::IWriter
    {
        std::string m_Data;

    public:
        omemstream(const unsigned aLen = File::DEFAULT_BUFFER_SIZE)
        {
            m_Data.reserve(aLen);
        }

        std::string&       str() { return m_Data; }
        const std::string& str() const { return m_Data; }

        void put(uint8_t aByte) { m_Data.push_back(aByte); }

        template <class T>
        void write(const T& t) { write(&t, sizeof(t)); }

        // File::IWriter
        void write(const void* aData, size_t aSize) override { m_Data.insert(m_Data.end(), (const uint8_t*)aData, (const uint8_t*)aData + aSize); }
        void flush() override {}
        void sync() override {}
        void close() override {}
    };
} // namespace Container
