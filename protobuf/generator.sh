#!/bin/sh

cat "$1" | `dirname "$0"`/generator.py | clang-format > "$2"