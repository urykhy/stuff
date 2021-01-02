#pragma once
#include <optional>

#include "cbor-internals.hpp"

namespace cbor {

    template <class T>
    void write(ostream& out, const std::optional<T>& t)
    {
        if (t.has_value())
            write(out, t.value());
        else
            write_special(out, CBOR_NULL);
    }

    template <class T>
    void read(istream& s, std::optional<T>& t)
    {
        TypeInfo info = get_type(s);
        if (info.major == CBOR_X and info.minor == CBOR_NULL) {
            t = std::nullopt;
            return;
        }
        s.unget();
        t.emplace();
        read(s, t.value());
    }
} // namespace cbor