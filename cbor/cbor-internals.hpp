#pragma once
#include <endian.h>
#include <string.h>

#include <cassert>
#include <cstdint>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <container/Stream.hpp>

#include "file/Reader.hpp"
#include "file/Writer.hpp"

namespace cbor {
    using binary = std::string;

    enum
    {
        CBOR_UINT   = 0,
        CBOR_NINT   = 1,
        CBOR_BINARY = 2,
        CBOR_STR    = 3,
        CBOR_LIST   = 4,
        CBOR_MAP    = 5,
        CBOR_TAG    = 6,
        CBOR_X      = 7,

        CBOR_FALSE  = 20,
        CBOR_TRUE   = 21,
        CBOR_8      = 24,
        CBOR_16     = 25,
        CBOR_32     = 26,
        CBOR_64     = 27,
        CBOR_FLOAT  = 26,
        CBOR_DOUBLE = 27,
    };

    using istream = File::IExactReader;
    using ostream = File::IWriter;

    using imemstream = Container::imemstream;
    using omemstream = Container::omemstream;
} // namespace cbor