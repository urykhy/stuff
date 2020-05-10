#pragma once

#include <cstdint>
#include <string.h>
#include <string>
#include <boost/utility/string_ref.hpp>

namespace Container
{
    typedef std::string binary;

    class imemstream
    {
        const boost::string_ref m_Data;
        size_t m_Offset = 0;
        void ensure(size_t aSize) { if (aSize + m_Offset > m_Data.size()) throw EndOfBuffer(); }
    public:

        struct EndOfBuffer : std::runtime_error { EndOfBuffer() : std::runtime_error("End of buffer") {} };

        imemstream(const binary& aBuffer): m_Data((const char*)&aBuffer[0], aBuffer.size()) { }

        boost::string_ref substring(size_t aSize) {
            ensure(aSize);
            boost::string_ref sResult = m_Data.substr(m_Offset, aSize);
            m_Offset += aSize;
            return sResult;
        }
        void read(void* aBuffer, size_t aSize) {
            ensure(aSize);
            memcpy(aBuffer, m_Data.data() + m_Offset, aSize);
            m_Offset += aSize;
        }
        void unget() {
            if (m_Offset == 0)
                throw EndOfBuffer();
            m_Offset--;
        }
        void skip(size_t aSize) {
            ensure(aSize);
            m_Offset += aSize;
        }
        bool empty() const { return m_Data.empty(); }

        boost::string_ref rest() { return substring(m_Data.size() - m_Offset); }
    };

    class omemstream
    {
        binary& m_Data;
    public:

        omemstream(binary& aData) : m_Data(aData) {}
        void put(uint8_t aByte) { m_Data.push_back(aByte); }
        void write(const void* aData, size_t aSize) {  m_Data.insert(m_Data.end(), (const uint8_t*)aData, (const uint8_t*)aData + aSize); }

        template<class T>
        void write(const T& t) { write(&t, sizeof(t)); }
    };
}
