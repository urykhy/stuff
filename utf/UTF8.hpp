#pragma once

#include <array>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include <mpl/Mpl.hpp>

namespace UTF8 {
    struct BadCharacter : std::invalid_argument
    {
        BadCharacter()
        : std::invalid_argument("invalid utf-8 character")
        {}
    };

    inline int trailSize(uint8_t c)
    {
        if (c <= 0x7F)
            return 0;
        if (c <= 0xBF)
            throw BadCharacter(); // Continuation byte
        if (c <= 0xDF)
            return 1;
        if (c <= 0xEF)
            return 2;
        if (c <= 0xF7)
            return 3;
        throw BadCharacter();
    }

    inline void checkTrail(uint8_t c)
    {
        if ((c & 0b11000000) != 0b10000000)
            throw BadCharacter();
    }

    class CharDecoder
    {
        const std::string_view m_Str;

    public:
        CharDecoder(std::string_view aStr)
        : m_Str(aStr)
        {}

        class Iterator
        {
            std::string_view m_Str;
            uint32_t         m_Char      = 0; // unicode character
            uint8_t          m_TrailSize = 0; // trail size for character
            uint8_t          m_XSize     = 0; // expected characters
            bool             m_Eol       = false;

            void init(uint8_t c)
            {
                m_Char      = 0;
                m_TrailSize = trailSize(c);
                m_XSize     = m_TrailSize;

                if (c <= 0x7F)
                    m_Char = c;
                else if (c <= 0xDF)
                    m_Char = c & 0x1F;
                else if (c <= 0xEF)
                    m_Char = c & 0x0F;
                else if (c <= 0xF7)
                    m_Char = c & 0x07;
            }

            void append(uint8_t c)
            {
                checkTrail(c);
                m_Char <<= 6;
                m_Char |= c & 0x3F;
            }

            // basic checks
            void check()
            {
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

            void step()
            {
                if (m_Str.empty()) {
                    m_Char = 0;
                    m_Eol  = true;
                    return;
                }
                while (!m_Str.empty()) {
                    const uint8_t sChar = (uint8_t)m_Str[0];

                    if (m_XSize > 0) {
                        m_XSize--;
                        append(sChar);
                    } else {
                        init(sChar);
                    }
                    if (m_XSize == 0) {
                        check();
                    }
                    m_Str.remove_prefix(1);
                    if (done())
                        return;
                }
                throw BadCharacter();
            }

            bool done() const { return m_XSize == 0; }

        public:
            using value_type        = uint32_t;
            using pointer           = uint32_t*;
            using reference         = uint32_t&;
            using iterator_category = std::input_iterator_tag;
            using difference_type   = uint32_t;

            Iterator(std::string_view aStr)
            : m_Str(aStr)
            {
                step();
            }

            Iterator& operator++()
            {
                step();
                return *this;
            }

            value_type operator*() const
            {
                return m_Char;
            }

            bool operator==(const Iterator& aOther) const { return m_Str == aOther.m_Str and m_Char == aOther.m_Char and m_Eol == aOther.m_Eol; }
            bool operator!=(const Iterator& aOther) const { return !(*this == aOther); }
        };

        Iterator begin() const
        {
            return Iterator(m_Str);
        }
        Iterator end() const
        {
            return Iterator({});
        }
    };

    // decode one character
    inline uint32_t Decode(std::string_view aStr)
    {
        CharDecoder sDecoder(aStr);
        auto        sIt = sDecoder.begin();
        if (sIt == sDecoder.end())
            throw std::invalid_argument("utf8 decode: string is empty");
        uint32_t sChar = *sIt;
        if (++sIt != sDecoder.end())
            throw std::invalid_argument("utf8 decode: not a single character");
        return sChar;
    }

    // validate string
    inline void Validate(std::string_view aStr)
    {
        for (auto x : CharDecoder(aStr))
            (void)x;
    }

    // string length in code points
    inline uint32_t Length(std::string_view aStr)
    {
        CharDecoder sDecoder(aStr);
        return std::distance(sDecoder.begin(), sDecoder.end());
    }

    // ensure characters from Basic Multilingual Plane
    inline bool IsBMP(std::string_view aStr)
    {
        for (auto x : CharDecoder(aStr))
            if (x > 0xFFFF)
                return false;
        return true;
    }

    // https://en.wikipedia.org/wiki/Plane_(Unicode)
    struct Range
    {
        uint32_t begin = 0;
        uint32_t end   = 0;
        bool     test(uint32_t x) const { return x >= begin and x <= end; }
    };

    inline constexpr Range BasicLatin{0x0000, 0x007F};
    inline constexpr Range BasicCyr{0x0400, 0x04FF};

    template <class... T>
    inline bool InRange(std::string_view aStr, T&&... aSet)
    {
        for (auto x : CharDecoder(aStr)) {
            bool sHit = false;
            Mpl::for_each_argument(
                [x, &sHit](auto range) {
                    if (!sHit)
                        sHit |= range.test(x);
                },
                aSet...);
            if (!sHit)
                return false;
        }
        return true;
    }

} // namespace UTF8
