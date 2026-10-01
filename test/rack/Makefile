# Focused spec 006 entry points reuse the production Rack harness and renderer.
RACK_DIR ?= ../../../..
RACK_DIR := $(abspath $(RACK_DIR))
.PHONY: super-echo inspect-super-echo
super-echo:
	$(MAKE) -C ../.. test-super-echo RACK_DIR="$(RACK_DIR)"
inspect-super-echo:
	$(MAKE) -C ../../tools/capture capture MODULE=SuperEcho RACK_DIR="$(RACK_DIR)"
