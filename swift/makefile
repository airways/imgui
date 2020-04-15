CFLAGS := -I".." -I"." -I"../examples" -I"./macos" -I"./linux" -I"../examples/libs/gl3w" -DIMGUI_IMPL_OPENGL_LOADER_GL3W $(CFLAGS)
CCFLAGS := -std=c++17 $(CFLAGS) $(CCFLAGS)
OBJCFLAGS := -fobjc-arc

CPP_SRCS := ../imgui_demo.cpp ../imgui_draw.cpp ../imgui_widgets.cpp ../imgui.cpp cimgui.cpp text.cpp
OBJC_SRCS := macos/runtime.mm ../examples/imgui_impl_osx.mm ../examples/imgui_impl_metal.mm
GLFW_CPP_SRCS := linux/runtime.cpp ../examples/imgui_impl_glfw.cpp ../examples/imgui_impl_opengl3.cpp
GLFW_C_SRCS := ../examples/libs/gl3w/GL/gl3w.c

CPP_SRC_OBJS := $(patsubst %.cpp,%.o,$(CPP_SRCS))
OBJC_SRC_OBJS := $(patsubst %.mm,%.o,$(OBJC_SRCS))

GLFW_CPP_SRC_OBJS := $(patsubst %.cpp,%.o,$(GLFW_CPP_SRCS))
GLFW_C_SRC_OBJS := $(patsubst %.c,%.o,$(GLFW_C_SRCS))

.PHONY: macos linux Gui

macos: libimgui_macos.a

linux: libimgui_linux.a

Gui: Gui.o Gui.swiftmodule

libimgui_macos.a: $(CPP_SRC_OBJS) $(OBJC_SRC_OBJS)
	$(AR) rcs $@ $^

libimgui_linux.a: $(CPP_SRC_OBJS) $(GLFW_CPP_SRC_OBJS) $(GLFW_C_SRC_OBJS)
	$(AR) rcs $@ $^

Gui.swiftmodule: Gui.swift
	swiftc -emit-module -module-name Gui Gui.swift -import-objc-header cimgui-bridging.h

Gui.o: Gui.swift
	swiftc -o Gui.o -module-name Gui -c Gui.swift -parse-as-library -import-objc-header cimgui-bridging.h

%.o: %.c
	$(CC) $< -o $@ -c $(CFLAGS)

%.o: %.cpp
	$(CC) $< -o $@ -c $(CCFLAGS)

%.o: %.mm
	$(CC) $< -o $@ -c $(CCFLAGS) $(OBJCFLAGS)

clean:
	rm -f *.o macos/*.o linux/*.o ../*.o ../examples/libs/gl3w/GL/*.o ../examples/*.o *.swiftmodule *.swiftdoc *.swiftsourceinfo
