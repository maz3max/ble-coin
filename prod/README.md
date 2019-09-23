# Production folder
This folder contains the Python3-script `gen_bond.py`, which helps creating the Keys and addresses for the central and peripherals.
Running the script initially will create two text files: `central.txt` and `coins.txt`.

## gen_bond.py
When executed, this script checks, if `central.txt` and `coins.txt` exist and populates them if needed. Every time this script runs, it adds a new coin line to `coins.txt`. Furthermore, it creates a hex-file for the generated coin bundeling the firmware and keys.
For actually registering coins to the central, please refer to the README in `central-onship/`.

## central.txt
This file remains unchanged after initial creation and contains one line with the **random BLE address** of the central and its identity resolving key (**IRK**).
It is initialized and used by `gen_bond.py`, so please do not edit it.

## coins.txt
This file contains address and key data for every coin, one line per coin. It is automagically filled when calling `gen_bond.py`.

