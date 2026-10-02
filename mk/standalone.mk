# Capture caller flags before plugin.mk adds Rack/ABI/optimization flags.
TEST_MODE ?= ordinary
ifneq ($(filter-out ordinary coverage asan-ubsan,$(TEST_MODE)),)
$(error TEST_MODE must be ordinary, coverage, or asan-ubsan)
endif
TEST_CXX := $(CXX)
TEST_CC := $(CC)
TEST_CFLAGS := $(CFLAGS) -O2
TEST_CPPFLAGS := $(CPPFLAGS) -Isrc -Idep/catch2-v3
TEST_FLAGS := $(CXXFLAGS) -std=c++14 -pthread -Wall -Wextra -Wno-unused-value
TEST_LINK := $(LDFLAGS)
ifeq ($(TEST_MODE),coverage)
TEST_FLAGS += -O0 -g -fprofile-instr-generate -fcoverage-mapping -femit-all-decls
else ifeq ($(TEST_MODE),asan-ubsan)
TEST_FLAGS += -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
endif
TEST_BUILD := .build/$(TEST_MODE)/dsp
TEST_SUFFIX := $(if $(filter Windows_NT,$(OS)),.exe)
DSP_SOURCES := $(sort $(wildcard test/dsp/*.cpp test/dsp/*/*.cpp))
DSP_ALIASES := $(DSP_SOURCES:.cpp=)
DSP_OBJECTS := $(patsubst test/%.cpp,$(TEST_BUILD)/%.o,$(DSP_SOURCES))
DSP_BINARIES := $(DSP_OBJECTS:.o=$(TEST_SUFFIX))
DSP_CATCH := $(TEST_BUILD)/catch.o
TEST_ARGS ?=
shell-quote = '$(subst ','"'"',$(1))'
.PHONY: FORCE test test-dsp test-build $(DSP_ALIASES)
FORCE:
test test-dsp: $(DSP_ALIASES)
test-build: $(DSP_BINARIES)
$(DSP_ALIASES): test/%: $(TEST_BUILD)/%$(TEST_SUFFIX)
	$< $(TEST_ARGS)
$(DSP_BINARIES): $(TEST_BUILD)/%$(TEST_SUFFIX): $(TEST_BUILD)/%.o $(DSP_CATCH)
	$(TEST_CXX) $(TEST_FLAGS) -o $@ $^ $(TEST_LINK)
$(DSP_OBJECTS): $(TEST_BUILD)/%.o: test/%.cpp
$(DSP_CATCH): dep/catch2-v3/catch_amalgamated.cpp
$(DSP_OBJECTS) $(DSP_CATCH): $(TEST_BUILD)/config Makefile mk/standalone.mk
	@mkdir -p $(@D)
	$(TEST_CXX) $(TEST_CPPFLAGS) $(TEST_FLAGS) -MMD -MP -c -o $@ $(firstword $(filter %.cpp,$^))
$(TEST_BUILD)/config: FORCE
	@python3 scripts/build-config.py $@ --compiler $(call shell-quote,$(TEST_CXX)) --flags $(call shell-quote,$(TEST_CPPFLAGS) $(TEST_FLAGS) $(TEST_LINK))
-include $(DSP_OBJECTS:.o=.d) $(DSP_CATCH:.o=.d)

# Only the OPM suite links the production core; production remains C++11.
$(TEST_BUILD)/dsp/yamaha_ym2151/test_voice$(TEST_SUFFIX): $(TEST_BUILD)/ymfm.o
$(TEST_BUILD)/ymfm.o: dep/ymfm/ymfm_opm.cpp $(TEST_BUILD)/config
	@mkdir -p $(@D)
	$(TEST_CXX) $(TEST_CPPFLAGS) $(TEST_FLAGS) -MMD -MP -c -o $@ $<
-include $(TEST_BUILD)/ymfm.d

# Nuked is an independent test reference, compiled as C, never in the plugin.
$(TEST_BUILD)/dsp/yamaha_ym2151/test_voice$(TEST_SUFFIX): $(TEST_BUILD)/nuked-opm.o
$(TEST_BUILD)/nuked-opm.o: dep/Nuked-OPM/opm.c dep/Nuked-OPM/opm.h $(TEST_BUILD)/c-config mk/standalone.mk
	@mkdir -p $(@D)
	$(TEST_CC) $(TEST_CFLAGS) -c -o $@ $<

$(TEST_BUILD)/c-config: FORCE
	@python3 scripts/build-config.py $@ --compiler $(call shell-quote,$(TEST_CC)) --flags $(call shell-quote,$(TEST_CFLAGS))
