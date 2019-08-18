# Production folder
This folder contains the Python3-script `gen_bond.py`, which helps creating the Keys and addresses for the central and peripherals.
Running the script initially will create two text files: `central.txt` and `coins.txt`.

Additionally, the `main.h` file containing the data for the first coin will be placed into the `factory-bonding-onchip/src/` directory.

## gen_bond.py
When executed, this script checks, if `central.txt` and `coins.txt` exist and populates them if needed. Every time this script runs, it adds a new coin line to `coins.txt`. Furthermore, it updates `factory-bonding-onchip/src/main.h` to include the data of the newest coin.

For actually registering coins to the central, please refer to the README in `central-onship/`.

## central.txt
This file remains unchanged after initial creation and contains one line with the **random BLE address** of the central and its identity resolving key (**IRK**).
It is initialized and used by `gen_bond.py`, so please do not edit it.

## coins.txt
This file contains address and key data for every coin, one line per coin. It is automagically filled when calling `gen_bond.py`.

## UICR_APPROTECT_clear.hex
Flashing this file activates the readback protection of the NRF52832 chip. It also deactivates the debugging interface, but the chip can be recovered by issuing a mass-erase.
