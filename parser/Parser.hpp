#pragma once
#include <boost/utility/string_ref.hpp>

namespace Parse
{
    // simple parser.
    //
    // parse N elements in string. drop tail. no quoting.
    template<class T>
    bool simple(boost::string_ref str, T& t, const char sep = ',')
    {
        for (size_t i = 0; i < t.size(); i++)
        {
            auto pos = str.find(sep);
            t[i] = str.substr(0, pos);
            str.remove_prefix(pos+1);
        }
        return true;
    }

    // complicated parser.
    //
    // parse csv format with quotes
    //
    // F must return std::string instance to collect characters
    //
    // supported 2 format:
    //   quotes + double quotes.
    //   escaping
    //
    template<class F>
    bool quoted(boost::string_ref str, F f, const char sep = ',', const char quote = '"', const char esc = '\\')
    {
        std::string* current = nullptr;

        bool first_character = true;
        bool quoted_string = false;
        bool skip_quote = false;
        bool esc_quote = false;

        char c = 0;
        for (size_t i = 0; i < str.size(); i++)
        {
            c = str[i];

            if (!current) {
                current = f();    // starting new token
                first_character = true; quoted_string = false; skip_quote = false; esc_quote = false;
            }

            if (first_character) {
                if (c == quote)      quoted_string = true;
                else if (c == sep)   current = nullptr;
                else                 current->push_back(c);
                first_character = false;
                continue;
            }
            if (esc_quote) {
                current->push_back(c);
                esc_quote = false;
                continue;
            }
            if (quoted_string)
            {
                if (skip_quote) {
                    skip_quote = false;
                    if (c == sep)           current = nullptr;          // ",
                    else if (c == quote)    current->push_back(quote);  // ""
                    else                    return false;               // "x
                } else {
                    if (c == esc)           esc_quote = true;
                    else if (c == quote)    skip_quote = true;
                    else                    current->push_back(c);
                }
            } else {
                if (c == esc)        esc_quote = true;
                else if (c == sep)   current = nullptr;
                else                 current->push_back(c);
            }
        }
        if (esc_quote)
            return false;
        if (quoted_string and !skip_quote and nullptr != current)
            return false;
        if (c == sep)   // if last char in string is separator - append empty token
            f();

        return true;
    }
}

