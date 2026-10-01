# Rack's public interface, with explicit SDK-free developer goals.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
include mk/standalone.mk

SDK_FREE_GOALS := test test-dsp test-build check-build clean clean-local test-coverage test-asan-ubsan
SDK_FREE_GOALS += $(DSP_ALIASES)
ifneq ($(strip $(filter-out $(SDK_FREE_GOALS),$(or $(MAKECMDGOALS),all))),)
RACK_DIR ?= ../..
FLAGS += -DTEST -Wno-unused-local-typedefs
SOURCES += $(wildcard src/dsp/*.cpp src/*.cpp)
DISTRIBUTABLES += LICENSE LICENSING.md docs/licenses res presets
# Preserve SDK link/package/install recipes, but isolate project objects.
override OBJECTS := $(patsubst %,.build/plugin/%.o,$(SOURCES))
override DEPENDENCIES := $(patsubst %,.build/plugin/%.d,$(SOURCES))
include $(RACK_DIR)/plugin.mk
include mk/rack.mk
clean: clean-local
else
.PHONY: clean
clean: clean-local
	rm -f plugin.so plugin.dylib plugin.dll
	rm -rf dist
endif

.PHONY: clean-local check-build test-coverage test-asan-ubsan
clean-local:
	rm -rf .build
check-build:
	python3 scripts/test-build.py

test-coverage test-asan-ubsan:
	python3 scripts/instrument.py $(patsubst test-%,%,$@)
