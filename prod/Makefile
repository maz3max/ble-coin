.PHONY: all
all: coin central pre

.PHONY: clean
clean:
	rm coin.hex central.hex pre.hex -f
	rm build/ -rf

.PHONY: coin
coin:
	west build -d build/coin ../coin/
	cp build/coin/zephyr/zephyr.hex coin.hex

.PHONY: central
central:
	west build -d build/central ../central-onchip/
	cp build/central/zephyr/zephyr.hex central.hex

.PHONY: pre
pre:
	west build -d build/pre ../factory-bonding-onchip/
	cp build/pre/zephyr/zephyr.hex pre.hex

