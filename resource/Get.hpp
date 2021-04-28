#pragma once

#include <string_view>

#define DECLARE_RESOURCE(name)                          \
extern "C" const char _binary_ ## name ## _start;    \
extern "C" const char _binary_ ## name ## _end;      \
namespace resource {                                    \
    inline std::string_view name () {                   \
        return std::string_view(&_binary_ ## name ## _start, &_binary_ ## name ## _end - &_binary_ ## name ## _start); \
    }                                                   \
}
