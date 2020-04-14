make
make Gui
set -x
./assemble.sh $1.o $1.swift
swiftc -o $1 $1.o Gui.o -limgui_macos -I"." -L"." -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit -lc++
