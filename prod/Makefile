.PHONY: all
all: coin coin-accelerometer coin-nrf51 central

.PHONY: clean
clean:
	rm coin.hex central.hex -f
	rm build/ -rf

.PHONY: coin
coin:		../.west/config
	BOARD=nrf52_coin BOARD_ROOT=../coin	west build -d build/coin ../coin/
	cp build/coin/zephyr/zephyr.hex coin.hex

.PHONY: coin-accelerometer
coin-accelerometer:		../.west/config
	BOARD=nrf52_coin BOARD_ROOT=../coin-accelerometer	west build -d build/coin-accelerometer ../coin-accelerometer/
	cp build/coin-accelerometer/zephyr/zephyr.hex coin-accelerometer.hex

.PHONY: coin-nrf51
coin-nrf51:		../.west/config
	BOARD=nrf51_coin BOARD_ROOT=../coin-nrf51	west build -d build/coin-nrf51 ../coin-nrf51/
	cp build/coin-nrf51/zephyr/zephyr.hex coin-nrf51.hex


.PHONY: central
central:	../.west/config
	BOARD=nrf52840_pca10059			west build -d build/central ../central-onchip/
	cp build/central/zephyr/zephyr.hex central.hex

../.west/config:
	west init ../
	west update
