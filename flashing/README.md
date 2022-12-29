# Flashing BLE Coins with a RPi Zero W 1

The coin is connected using a programming jig
that controls power output with a transistor (active-low).

The following pins are used:

* 3.3V
* GND
* swclk = GPIO 11
* swdio = GPIO 25
* pwr_en = GPIO 27

The `openocd` package needs to be installed for this to work.

While flashing, the coins are put in a protected mode. You can query that mode with `./check.sh` and factory-reset a coin with `./unlock.sh`.

Flashing is done with `./flash.sh something.hex`.
