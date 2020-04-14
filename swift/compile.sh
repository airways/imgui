make clean
make
make Gui
./build.sh main.o main.swift
swiftc -o main main.o Gui.o -limgui_macos -I"." -L"." -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit -lc++
