#pragma once

#include <string_view>
#include <vector>

#include <file/File.hpp>
#include <string/String.hpp>
#include <unsorted/Raii.hpp>

namespace Parser::CSV {
    // complicated parser.
    //
    // parse csv format with quotes
    // call f for every element
    // supported 2 format:
    //   quotes + double quotes.
    //   escaping

    template <class F>
    bool line(std::string_view str, F f, const char sep = ',', const char quote = '"', const char esc = '\\')
    {
        std::string current;

        bool first_character = true;
        bool quoted_string   = false;
        char previous        = 0;
        char c               = 0;
        for (size_t i = 0; i < str.size(); i++) {
            c = str[i];
            if (previous == sep) {
                f(current); // starting new token
                current.clear();
                first_character = true;
                quoted_string   = false;
                previous        = 0;
            }
            if (first_character) {
                first_character = false;
                if (c == quote) {
                    quoted_string = true;
                    continue;
                }
            }
            if (previous == esc) {
                current.push_back(c);
                previous = 0;
                continue;
            }
            if (quoted_string) {
                if (previous == quote) {
                    previous = 0;
                    if (c == sep)
                        previous = sep; // ",
                    else if (c == quote)
                        current.push_back(quote); // ""
                    else
                        return false; // "x
                } else {
                    if (c == esc)
                        previous = esc;
                    else if (c == quote)
                        previous = quote;
                    else
                        current.push_back(c);
                }
            } else {
                if (c == esc)
                    previous = esc;
                else if (c == sep)
                    previous = sep;
                else
                    current.push_back(c);
            }
        }
        if (previous == esc)
            return false;
        if (quoted_string and (previous != sep and previous != quote))
            return false;
        f(current);
        if (c == sep) // if last char in string is separator - append empty token
        {
            current.clear();
            f(current);
        }

        return true;
    }

    // list of column names
    using Header = std::vector<std::string>;

    // parse header line
    inline Header header(std::string_view aHeader)
    {
        Header sHeader;
        if (!line(aHeader, [&sHeader](std::string& aStr) mutable {
                String::trim(aStr);
                sHeader.push_back(aStr);
            }))
            throw std::runtime_error("bad csv header line");
        return sHeader;
    }

    // parse data line to object
    template <class T, class M>
    void object(std::string_view aStr, const M& sIndex, T& aObj)
    {
        unsigned sCounter = 0;
        if (!line(aStr, [&sCounter, &sIndex, &aObj](std::string& aStr) mutable {
                Util::Raii sInc([&]() { sCounter++; });
                if (sCounter >= sIndex.size())
                    return;
                const auto& sCallback = sIndex[sCounter];
                if (!sCallback)
                    return;
                String::trim(aStr);
                sCallback(aStr, aObj);
            }))
            throw std::runtime_error("bad csv data line");
    }

    // parse file with header
    template <class F, class C>
    void from_file(const std::string& aFilename, F&& aTest, C& aList)
    {
        using T      = typename C::value_type;
        bool sHeader = true;

        typename T::Index sIndex;
        File::by_string(aFilename, [&sHeader, &sIndex, &aTest, &aList](std::string_view aStr) mutable {
            if (sHeader) {
                sIndex   = T::prepare(header(aStr));
                sHeader = false;
            } else {
                T sTmp;
                object(aStr, sIndex, sTmp);
                if (aTest(sTmp))
                    aList.push_back(std::move(sTmp));
            }
        });
    }

} // namespace Parser::CSV