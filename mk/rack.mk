# SDK-backed tests keep Rack ABI flags but use a C++14 Catch2 harness.
RACK_TEST_BUILD := .build/ordinary/rack
RACK_TEST_FLAGS := $(filter-out -std=% -municode,$(CXXFLAGS)) -std=c++14 -Idep/catch2-v3
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
	$(CXX) $(RACK_TEST_FLAGS) -MMD -MP -c -o $@ $(filter %.cpp,$^)
.build/plugin/%.cpp.o: %.cpp .build/plugin/config Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
.build/plugin/config: FORCE
	@python3 scripts/build-config.py $@ --compiler $(call shell-quote,$(CXX)) --flags $(call shell-quote,$(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)) --sdk $(call shell-quote,$(RACK_DIR))
$(RACK_TEST_BUILD)/config: FORCE
	@python3 scripts/build-config.py $@ --compiler $(call shell-quote,$(CXX)) --flags $(call shell-quote,$(RACK_TEST_FLAGS) $(RACK_TEST_LINK)) --sdk $(call shell-quote,$(RACK_DIR))
-include $(RACK_TEST_OBJECTS:.o=.d) $(RACK_TEST_BUILD)/catch.d
