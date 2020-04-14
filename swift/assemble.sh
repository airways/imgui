#!/usr/bin/env bash

clang -o macos/macosWrapper.o -c macos/macosWrapper.c
swiftc -o tmp-$1 -c $2 -I"."
ld -r -o $1 tmp-$1 macos/macosWrapper.o -unexported_symbols_list macos/unexported.lst
rm -f tmp-$1

