#pragma once
#include <stdexcept>
#include <sstream>
#include <string.h>

namespace Util {

    template<class T>
    std::string append_errno(T msg, int err) {
        std::stringstream s;
        s << msg << " (errno " << err << ": " << strerror(err) << ")";
        return s.str();
    }

    /*
     * usage:
     *
     * struct rpc_base;
     * struct rpc_not_found;
     * typedef TaggedException<rpc_base> Exception;
     * typedef TaggedException<rpc_not_found, Exception> e_not_found;
     *
     */

    template<class T, class B = std::runtime_error>
    struct TaggedException : B {
        TaggedException(const std::string s) : B(s) {}
        TaggedException(const std::string s, int err) : B(append_errno(s, err)) {}
    };


} // namespace Util
