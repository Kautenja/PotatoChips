FLAGS += \
	-DTEST \
	-Wno-unused-local-typedefs

SOURCES += $(wildcard src/dsp/*.cpp)
SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += LICENSE LICENSING.md docs/licenses res presets

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
