#pragma once
#include <string_view>

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
} // namespace Parser
