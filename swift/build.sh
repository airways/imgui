#!/usr/bin/env bash

SOURCE="${BASH_SOURCE[0]}"
DIR=`dirname ${SOURCE}`
UNAME_S=`uname -s`

make Gui -C ${DIR}

if [ "${UNAME_S}" = "Darwin" ]; then
  make macos -C ${DIR}
  set -x
  ${DIR}/assemble.sh $1.o $1.swift
  swiftc -o $1 $1.o ${DIR}/Gui.o -limgui_macos -I"${DIR}" -L"${DIR}" -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit -lc++
  sed 's/EXECUTABLE\_NAME/main/g' ${DIR}/macos/Info.plist > `dirname $1`/Info.plist
fi

if [ "${UNAME_S}" = "Linux" ]; then
  make linux -C ${DIR}
  set -x
  ${DIR}/assemble.sh $1.o $1.swift
  swiftc -o $1 $1.o ${DIR}/Gui.o -limgui_linux -I"${DIR}" -L"${DIR}" -lstdc++ -lpthread -lGL `pkg-config --static --libs glfw3`
fi
