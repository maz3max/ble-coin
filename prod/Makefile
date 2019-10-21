.PHONY: all
all: coin central

.PHONY: clean
clean:
	rm coin.hex central.hex -f
	rm build/ -rf

.PHONY: coin
coin:		../.west/config
	west build -d build/coin ../coin/
	cp build/coin/zephyr/zephyr.hex coin.hex

.PHONY: central
central:	../.west/config
	west build -d build/central ../central-onchip/
	cp build/central/zephyr/zephyr.hex central.hex

../.west/config:
	west init ../
	west update
