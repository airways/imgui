#!/usr/bin/env bash

SOURCE="${BASH_SOURCE[0]}"
DIR=`dirname ${SOURCE}`

clang -o ${DIR}/macos/macosWrapper.o -c ${DIR}/macos/macosWrapper.c
swiftc -o tmp-$1 -c $2 -I"${DIR}"
ld -r -o $1 tmp-$1 ${DIR}/macos/macosWrapper.o -unexported_symbols_list ${DIR}/macos/unexported.lst
rm -f tmp-$1

