#!/bin/sh

wget "$1" -O - | iconv -f windows-1251 -t utf-8 | dos2unix | ./samlib2fb2.py


