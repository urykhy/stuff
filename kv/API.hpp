#pragma once

#include <stdint.h>

#include <optional>
#include <string>

#include <cbor/cbor.hpp>

namespace KV {
    struct Request
    {
        std::string                key;
        std::optional<std::string> value = {};

        void cbor_read(cbor::istream& sInput)
        {
            cbor::read(sInput, key);
            cbor::read(sInput, value);
        }
        void cbor_write(cbor::ostream& sOutput) const
        {
            cbor::write(sOutput, key);
            cbor::write(sOutput, value);
        }
    };
    using Requests = std::vector<Request>;

    struct Response
    {
        std::string                key;
        std::optional<std::string> value;

        void cbor_read(cbor::istream& sInput)
        {
            cbor::read(sInput, key);
            cbor::read(sInput, value);
        }
        void cbor_write(cbor::ostream& sOutput) const
        {
            cbor::write(sOutput, key);
            cbor::write(sOutput, value);
        }
    };
    using Responses = std::vector<Response>;

} // namespace KV