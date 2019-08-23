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
	@rm -f ../prod/*.hex
	$(foreach sub, $(patsubst %/Makefile, %, $(SUBDIRS)), $(MAKE) -C $(sub) clean;)
