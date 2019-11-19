#pragma once

#include <string>

namespace UTF8
{
    struct BadCharacter {};

    inline int trailSize(char cs)
    {
        unsigned char c = (unsigned char)cs;
        if (c <= 0x7F) return 0;
        if (c <= 0xBF) throw BadCharacter(); // Continuation byte
        if (c <= 0xDF) return 1;
        if (c <= 0xEF) return 2;
        if (c <= 0xF7) return 3;
        throw BadCharacter();
    }

    inline void checkTrail(char cs)
    {
        unsigned char c = (unsigned char)cs;
        if ((c & 0b11000000) != 0b10000000)
            throw BadCharacter();
    }

    class CharDecoder
    {
        uint32_t m_Char = 0;
        int m_TrailSize = 0; // trail size for character
        int m_XSize     = 0; // expected characters

        void init(char cs)
        {
            m_Char = 0;
            m_TrailSize = trailSize(cs);
            m_XSize = m_TrailSize;

            unsigned char c = (unsigned char)cs;
            if (c <= 0x7F) m_Char = c;
            else if (c <= 0xDF) m_Char = c & 0x1F;
            else if (c <= 0xEF) m_Char = c & 0x0F;
            else if (c <= 0xF7) m_Char = c & 0x07;
            //std::cout << "init char is " << std::hex << m_Char << " ( from " << (unsigned)(unsigned char)cs << ")" << std::endl;
        }

        void append(char cs)
        {
            checkTrail(cs);
            unsigned char c = (unsigned char)cs;
            m_Char <<= 6;
            m_Char |= c & 0x3F;
        }

        void check()
        {
            //std::cout << "test: collected char is " << std::hex << m_Char << std::endl;
            // ensure minimal length coding
            if (m_TrailSize > 0 and m_Char <= 0x7F)
                throw BadCharacter();
            if (m_TrailSize > 1 and m_Char <= 0x7FF)
                throw BadCharacter();
            if (m_TrailSize > 2 and m_Char <= 0xFFFF)
                throw BadCharacter();
            if (m_Char > 0x1FFFFF)
                throw BadCharacter();
            // detect UTF-16 surrogates
            if (m_Char >= 0xD800 and m_Char <= 0xDFFF)
                throw BadCharacter();
        }
    public:

        void push_back(char cs)
        {
            if (m_XSize > 0)
            {
                m_XSize--;
                append(cs);
                if (m_XSize == 0)
                    check();
            } else {
                init(cs);
            }
        }

        bool done() const { return m_XSize == 0; }
        uint32_t sym() const { return m_Char; }
    };

    inline uint32_t Validate(const std::string& aStr)
    {
        CharDecoder sDecoder;
        for (const char a : aStr)
            sDecoder.push_back(a);
        if (!sDecoder.done())
            throw BadCharacter();
        return sDecoder.sym();
    }
}

