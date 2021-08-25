#pragma once

#include <string_view>

// based on https://gist.github.com/mmozeiko/ed9655cf50341553d282

// clang-format off
#define DECLARE_RESOURCE(name, file)                                                                            \
    __asm__(".section .rodata\n"                                                                                \
            ".global _binary_" #name "_start\n"                                                                 \
            ".balign 16\n"                                                                                      \
            "_binary_" #name "_start:\n"                                                                        \
            ".incbin \"" file "\"\n"                                                                            \
            ".global _binary_" #name "_end\n"                                                                   \
            ".balign 1\n"                                                                                       \
            "_binary_" #name "_end:\n");                                                                        \
    extern "C" const __attribute__((aligned(16))) char _binary_##name##_start;                                  \
    extern "C" const char                              _binary_##name##_end;                                    \
    namespace resource {                                                                                        \
        inline std::string_view name()                                                                          \
        {                                                                                                       \
            return std::string_view(&_binary_##name##_start, &_binary_##name##_end - &_binary_##name##_start);  \
        }                                                                                                       \
    }

// clang-format on