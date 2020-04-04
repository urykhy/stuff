#pragma once

#include <string_view>

#define DECLARE_RESOURCE(name)                          \
extern "C" const char _binary____ ## name ## _start;    \
extern "C" const char _binary____ ## name ## _end;      \
namespace resource {                                    \
    inline std::string_view name () {                   \
        return std::string_view(&_binary____ ## name ## _start, &_binary____ ## name ## _end - &_binary____ ## name ## _start); \
    }                                                   \
}
