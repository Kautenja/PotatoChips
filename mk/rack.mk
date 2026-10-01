# SDK-backed tests keep Rack ABI flags but use a C++14 Catch2 harness.
RACK_TEST_MODE ?= ordinary
ifneq ($(filter-out ordinary asan-ubsan,$(RACK_TEST_MODE)),)
$(error RACK_TEST_MODE must be ordinary or asan-ubsan)
endif
RACK_TEST_BUILD := .build/$(RACK_TEST_MODE)/rack
RACK_TEST_FLAGS := $(filter-out -std=% -municode,$(CXXFLAGS)) -std=c++14 -Idep/catch2-v3
ifeq ($(RACK_TEST_MODE),asan-ubsan)
RACK_TEST_FLAGS := $(filter-out -O% -funsafe-math-optimizations,$(RACK_TEST_FLAGS)) -O1 -fsanitize=address,undefined -fno-sanitize-recover=all
endif
RACK_TEST_LINK := -L$(RACK_DIR) -lRack $(EXTRA_TEST_LDFLAGS)
RACK_TEST_SUFFIX := $(if $(ARCH_WIN),.exe)
RACK_TEST_SOURCES := $(wildcard test/rack/*.cpp)
RACK_TEST_OBJECTS := $(patsubst test/rack/%.cpp,$(RACK_TEST_BUILD)/%.o,$(RACK_TEST_SOURCES))
RACK_TEST_BINARIES := $(RACK_TEST_OBJECTS:.o=$(RACK_TEST_SUFFIX))
.PHONY: test-rack
test-rack: $(RACK_TEST_BINARIES)
	@set -e; for suite in $(RACK_TEST_BINARIES); do \
		DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" "$$suite" $(TEST_ARGS); \
	done
$(RACK_TEST_BINARIES): $(RACK_TEST_BUILD)/%$(RACK_TEST_SUFFIX): $(RACK_TEST_BUILD)/%.o $(RACK_TEST_BUILD)/catch.o
	$(CXX) $(RACK_TEST_FLAGS) -o $@ $^ $(RACK_TEST_LINK)
$(RACK_TEST_OBJECTS): $(RACK_TEST_BUILD)/%.o: test/rack/%.cpp
$(RACK_TEST_BUILD)/catch.o: dep/catch2-v3/catch_amalgamated.cpp
$(RACK_TEST_OBJECTS) $(RACK_TEST_BUILD)/catch.o: $(RACK_TEST_BUILD)/config Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(RACK_TEST_FLAGS) -MD -MP -c -o $@ $(firstword $(filter %.cpp,$^))
.build/plugin/%.cpp.o: %.cpp .build/plugin/config Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
.build/plugin/config: FORCE
	@python3 scripts/build-config.py $@ --compiler $(call shell-quote,$(CXX)) --flags $(call shell-quote,$(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)) --sdk $(call shell-quote,$(RACK_DIR))
$(RACK_TEST_BUILD)/config: FORCE
	@python3 scripts/build-config.py $@ --compiler $(call shell-quote,$(CXX)) --flags $(call shell-quote,$(RACK_TEST_FLAGS) $(RACK_TEST_LINK)) --sdk $(call shell-quote,$(RACK_DIR))
-include $(RACK_TEST_OBJECTS:.o=.d) $(RACK_TEST_BUILD)/catch.d

# Contract tests link the production registration and all model implementations.
$(RACK_TEST_BUILD)/test_contract$(RACK_TEST_SUFFIX): $(OBJECTS)
