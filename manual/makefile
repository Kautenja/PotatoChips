# Stable release filenames; blanks and disabled models have no manuals.
.DEFAULT_GOAL := all
MODULES := SuperEcho InfiniteStairs BossFight MiniBoss NameCorpOctalWaveGenerator PalletTownWavesSystem Blocks Jairasullator MegaTone PotKeys Pulses StepSaw SuperADSR SuperVCA
.PHONY: all manual clean $(MODULES)
all: manual
manual: $(MODULES)
$(MODULES):
	@mkdir -p .build
	rm -f ".build/$@.pdf"
	$(MAKE) -C "$@"
	cp "$@/.build/manual.pdf" ".build/$@.pdf"
clean:
	@set -e; for module in $(MODULES); do $(MAKE) -C "$$module" clean; done
	rm -f $(addprefix .build/,$(addsuffix .pdf,$(MODULES)))
