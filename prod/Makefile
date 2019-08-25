SUBDIRS = $(wildcard ../build/*/Makefile)
HEXFILES = $(patsubst ../build/%/Makefile, %.hex, $(SUBDIRS))

.PHONY: all
all: $(HEXFILES)

.PRECIOUS: ../build/%/zephyr/zephyr.hex
../build/%/zephyr/zephyr.hex:
	$(MAKE) -C $(shell dirname $@)

%.hex: ../build/%/zephyr/zephyr.hex
	cp $^ $@

.PHONY: clean
clean:
	@find *.hex -not -name 'UICR_APPROTECT_clear.hex' -delete
	$(foreach sub, $(patsubst %/Makefile, %, $(SUBDIRS)), rm -f $(sub)/zephyr/zephyr.*;)

.PHONY: squeaky-clean
squeaky-clean:
	@find *.hex -not -name 'UICR_APPROTECT_clear.hex' -delete
	$(foreach sub, $(patsubst %/Makefile, %, $(SUBDIRS)), $(MAKE) -C $(sub) clean;)
