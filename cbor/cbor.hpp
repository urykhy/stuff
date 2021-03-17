#pragma once

// clang-format off
#include "cbor-custom.hpp"
#include "cbor-stl.hpp"
#include "cbor-optional.hpp"
#include "cbor-multi-index.hpp"
#include "cbor-tuple.hpp"

#include "cbor-proxy.hpp"
// clang-format on

namespace cbor {
    template <class... T>
    inline std::string to_string(const T&... t)
    {
        cbor::omemstream sStream;
        cbor::write(sStream, t...);
        return sStream.str();
    }

    template <class... T>
    inline void from_string(const std::string& aStr, T&... t)
    {
        cbor::imemstream sStream(aStr);
        cbor::read(sStream, t...);
    }
} // namespace cbor
