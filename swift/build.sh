#!/usr/bin/env bash
set -euo pipefail

SOURCE="${BASH_SOURCE[0]}"
DIR=`dirname ${SOURCE}`
UNAME_S=`uname -s`
NAME=`basename $1 .swift`

make Gui -C ${DIR}

if [ "${UNAME_S}" = "Darwin" ]; then
  make macos -C ${DIR}
  set -x
  ${DIR}/assemble.sh $NAME.o $1
  swiftc -o $NAME $NAME.o ${DIR}/Gui.o -limgui_macos -I"${DIR}" -L"${DIR}" -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit -lc++
  sed 's/EXECUTABLE\_NAME/main/g' ${DIR}/macos/Info.plist > `dirname $NAME`/Info.plist
  set +x
fi

if [ "${UNAME_S}" = "Linux" ]; then
  make linux -C ${DIR}
  set -x
  ${DIR}/assemble.sh $NAME.o $1
  swiftc -o $NAME $NAME.o ${DIR}/Gui.o -limgui_linux -I"${DIR}" -L"${DIR}" -lstdc++ -lpthread -lGL `pkg-config --static --libs glfw3`
  set +x
fi
