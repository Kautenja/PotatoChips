# Optional production-widget renderer; no audio device or installed plugin needed.
# Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
.DEFAULT_GOAL := capture
RACK_DIR ?= ../../../..
RACK_DIR := $(abspath $(RACK_DIR))
include $(RACK_DIR)/arch.mk
BUILD := .build
PYTHON ?= python3
CPPFLAGS += -isystem $(RACK_DIR)/include -isystem $(RACK_DIR)/dep/include
CXXFLAGS += -std=c++11 -O2 -g -DTEST -Wno-unused-local-typedefs
CFLAGS += -O2
ifdef ARCH_X64
CXXFLAGS += -march=nehalem
CFLAGS += -march=nehalem
endif
ifdef ARCH_MAC
GL_LIBS := -framework OpenGL
else ifdef ARCH_WIN
$(error Capture currently supports macOS and Linux desktop sessions)
else
GL_LIBS := -lGL
endif
SOURCES := $(wildcard ../../src/*.cpp ../../src/dsp/*.cpp) ../../dep/ymfm/ymfm_opm.cpp
OBJECTS := $(patsubst ../../%.cpp,$(BUILD)/%.o,$(SOURCES))
.PHONY: capture screenshots drawings test test-native
drawings: capture
	$(PYTHON) draw_panels.py $(BUILD)/captures $(if $(MODULE),--module "$(MODULE)")
test:
	$(PYTHON) test_capture.py
test-native: $(BUILD)/capture
	$(PYTHON) test_native.py --rack "$(RACK_DIR)" --build "$(BUILD)"
capture: $(BUILD)/capture
	@mkdir -p $(BUILD)/user
	$(PYTHON) run.py --rack "$(RACK_DIR)" --build "$(BUILD)" $(if $(MODULE),--module "$(MODULE)")
screenshots: capture
	$(PYTHON) export_screenshots.py $(BUILD)/captures ../../manual $(if $(MODULE),--module "$(MODULE)")
$(BUILD)/capture: $(BUILD)/capture.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -L$(RACK_DIR) -lRack $(GL_LIBS) -o $@
$(BUILD)/capture.o: capture.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -c $< -o $@
$(BUILD)/%.o: ../../%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -c $< -o $@
$(BUILD)/%.o: ../../%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MD -MP -c $< -o $@
-include $(OBJECTS:.o=.d) $(BUILD)/capture.d

# Keep a different SDK/compiler from silently reusing an old capture executable.
.PHONY: FORCE
FORCE:
$(BUILD)/config: FORCE
	$(PYTHON) ../../scripts/build-config.py $@ --compiler '$(CXX)' --flags '$(CPPFLAGS) $(CXXFLAGS) $(GL_LIBS)' --sdk '$(RACK_DIR)'
$(OBJECTS) $(BUILD)/capture.o: $(BUILD)/config Makefile

# Optional real Fundamental Scope; ordinary builds have no Fundamental dependency.
FUNDAMENTAL_DIR ?= $(RACK_DIR)/plugins/Fundamental
.PHONY: adsr-scope
adsr-scope: $(BUILD)/adsr_scope
	@mkdir -p $(BUILD)/adsr-scope/user
	DYLD_LIBRARY_PATH="$(RACK_DIR)" LD_LIBRARY_PATH="$(RACK_DIR)" $< "$(RACK_DIR)" "$(abspath ../..)" "$(FUNDAMENTAL_DIR)" "$(abspath $(BUILD)/adsr-scope)"
$(BUILD)/adsr_scope: $(BUILD)/adsr_scope.o $(BUILD)/src/SuperADSR.o
	$(CXX) $(CXXFLAGS) $^ -L$(RACK_DIR) -lRack $(GL_LIBS) $(if $(ARCH_LIN),-ldl) -o $@
$(BUILD)/adsr_scope.o: adsr_scope.cpp $(BUILD)/config Makefile
	@mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -c $< -o $@
-include $(BUILD)/adsr_scope.d
