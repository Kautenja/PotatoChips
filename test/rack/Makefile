# Focused spec 006/007 entry points reuse the production Rack harness and renderer.
RACK_DIR ?= ../../../..
RACK_DIR := $(abspath $(RACK_DIR))
.PHONY: super-echo inspect-super-echo
super-echo:
	$(MAKE) -C ../.. test-super-echo RACK_DIR="$(RACK_DIR)"
inspect-super-echo:
	$(MAKE) -C ../../tools/capture capture MODULE=SuperEcho RACK_DIR="$(RACK_DIR)"

.PHONY: super-adsr
super-adsr:
	$(MAKE) -C ../.. test-super-adsr RACK_DIR="$(RACK_DIR)"
