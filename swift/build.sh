SOURCE="${BASH_SOURCE[0]}"
DIR=`dirname ${SOURCE}`
make -C ${DIR}
make Gui -C ${DIR}
set -x
${DIR}/assemble.sh $1.o $1.swift
swiftc -o $1 $1.o ${DIR}/Gui.o -limgui_macos -I"${DIR}" -L"${DIR}" -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit -lc++
