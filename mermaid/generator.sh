#!/bin/sh

`dirname "$0"`/generator.py $1 | clang-format > "$2"
