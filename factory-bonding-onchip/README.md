# Factory Bonding Firmware
This folder contains firmware that writes the **BLE bond** and the **SPACEKEY** into the flash of the coin. It is supposed to be overwritten by the **coin firmware** afterwards.

## Usage
After creating the `main.h` file using the Python script located in `prod\`, the firmware can be compiled and flashed.
When connected via an STLinkV2, flashing can be as simple as calling `make flash` in the build directory.
