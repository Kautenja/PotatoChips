# Source-relative manual builds; no Rack SDK or graphical session required.
.DEFAULT_GOAL := manual
BUILD ?= .build
LATEXMK ?= latexmk
PYTHON ?= python3
export TEXINPUTS := ../latex//:$(TEXINPUTS):
.PHONY: manual clean
manual:
	$(PYTHON) ../latex/check-manual.py output "$(BUILD)"
	@mkdir -p "$(BUILD)"
	@set -e; \
	trap 'result=$$?; if [ $$result -ne 0 ]; then rm -f "$(BUILD)/manual.pdf"; fi; exit $$result' 0; \
	rm -f "$(BUILD)/manual.pdf"; \
	$(PYTHON) ../latex/check-manual.py source manual.tex; \
	$(LATEXMK) -norc -pdf -pdflatex='pdflatex -no-shell-escape %O %S' -interaction=nonstopmode -halt-on-error -file-line-error -outdir="$(BUILD)" manual.tex; \
	$(PYTHON) ../latex/check-manual.py log "$(BUILD)/manual.log"; \
	$(LATEXMK) -norc -c -outdir="$(BUILD)" manual.tex; \
	rm -f "$(BUILD)/manual.bbl"
clean:
	$(PYTHON) ../latex/check-manual.py output "$(BUILD)"
	$(LATEXMK) -norc -C -outdir="$(BUILD)" manual.tex
	rm -f "$(BUILD)/manual.bbl"
