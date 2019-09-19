.PHONY: all
all: coin central pre

.PHONY: clean
clean:
	rm coin.hex central.hex -f
	rm build/ -rf

.PHONY: coin
coin:
	west build -d build/coin ../coin/ --board nrf52840_pca10059
	cp build/coin/zephyr/zephyr.hex coin.hex

.PHONY: central
central:
	west build -d build/central ../central-onchip/ --board nrf52840_pca10059
	cp build/central/zephyr/zephyr.hex central.hex
