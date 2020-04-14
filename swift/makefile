CCFLAGS := -std=c++14 -fobjc-arc -I".." -I"../examples" -I"./macos" $(CCFLAGS)

CPP_SRCS := ../imgui_demo.cpp ../imgui_draw.cpp ../imgui_widgets.cpp ../imgui.cpp cimgui.cpp
OBJC_SRCS := macos/runtime.mm ../examples/imgui_impl_osx.mm ../examples/imgui_impl_metal.mm

CPP_SRC_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRCS))
OBJC_SRC_OBJS := $(patsubst %.mm,%.o,$(OBJC_SRCS))

.PHONY: all Gui

all: libimgui_macos.a

libimgui_macos.a: $(CPP_SRC_OBJS) $(OBJC_SRC_OBJS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CC) $< -o $@ -c $(CCFLAGS)

%.o: %.mm
	$(CC) $< -o $@ -c $(CCFLAGS)

Gui:
	swiftc -emit-module -module-name Gui Gui.swift -import-objc-header cimgui-bridging.h
	swiftc -o Gui.o -module-name Gui -c Gui.swift -parse-as-library -import-objc-header cimgui-bridging.h

clean:
	rm -f *.o macos/*.o ../*.o ../examples/*.o *.swiftmodule *.swiftdoc *.swiftsourceinfo
