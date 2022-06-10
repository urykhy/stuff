#!/bin/bash

`dirname "$0"`/swagger.py $1 && clang-format -i `basename "${1/.yaml/.hpp}"` `basename "${1/.yaml/.cpp}"`