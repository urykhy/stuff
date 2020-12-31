#!/bin/sh

`dirname "$0"`/swagger.py $1 | clang-format > "$2"
