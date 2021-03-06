#pragma once
#include <string_view>
#include <boost/utility/string_ref.hpp>

namespace Parser {
    // simple parser.
    //
    // call t for every substring
    template <class T>
    bool simple(std::string_view str, T&& t, const char sep = ',')
    {
        while (!str.empty()) {
            auto pos = str.find(sep);
            if (pos == std::string_view::npos)
                pos = str.size();
            t(str.substr(0, pos));
            str.remove_prefix(std::min(pos + 1, str.size()));
        }
        return true;
    }

    inline bool simple(std::string_view str, std::vector<std::string_view>& result, const char sep = ',')
    {
        return simple(
            str, [&result](const auto& s) { result.push_back(s); }, sep);
    }

    // complicated parser.
    //
    // parse csv format with quotes
    // call f for every element
    // supported 2 format:
    //   quotes + double quotes.
    //   escaping
    //
    template <class F>
    bool quoted(boost::string_ref str, F f, const char sep = ',', const char quote = '"', const char esc = '\\')
    {
        bool        next_token = false;
        std::string current;

        bool first_character = true;
        bool quoted_string   = false;
        bool skip_quote      = false;
        bool esc_quote       = false;

        char c = 0;
        for (size_t i = 0; i < str.size(); i++) {
            c = str[i];

            if (next_token) {
                f(current); // starting new token
                current.clear();
                next_token      = false;
                first_character = true;
                quoted_string   = false;
                skip_quote      = false;
                esc_quote       = false;
            }
            if (first_character) {
                first_character = false;
                if (c == quote) {
                    quoted_string = true;
                    continue;
                }
            }
            if (esc_quote) {
                current.push_back(c);
                esc_quote = false;
                continue;
            }
            if (quoted_string) {
                if (skip_quote) {
                    skip_quote = false;
                    if (c == sep)
                        next_token = true; // ",
                    else if (c == quote)
                        current.push_back(quote); // ""
                    else
                        return false; // "x
                } else {
                    if (c == esc)
                        esc_quote = true;
                    else if (c == quote)
                        skip_quote = true;
                    else
                        current.push_back(c);
                }
            } else {
                if (c == esc)
                    esc_quote = true;
                else if (c == sep)
                    next_token = true;
                else
                    current.push_back(c);
            }
        }
        if (esc_quote)
            return false;
        if (quoted_string and !skip_quote and !next_token)
            return false;
        f(current);
        if (c == sep) // if last char in string is separator - append empty token
        {
            current.clear();
            f(current);
        }

        return true;
    }
} // namespace Parser
