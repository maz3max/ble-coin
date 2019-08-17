# Coin Firmware
This directory contains the firmware that creates a **BLE peripheral** loading the **BLE bond** and the **SPACEKEY** from flash using zephyr's settings API.

## Usage
Please refer to the top level README file for flashing instructions. Especially, this firmware will **not** work properly when flashed onto a virgin, mass-erased chip.
When connected via an STLinkV2, flashing can be as simple as calling `make flash` in the build directory.

When flashed, press the button on the coin to wake it up. The LED will light up and the coin will send **BLE advertisements** to the central signaling that it wants to connect.

When finished authenticating, on connection loss or when a timeout of 10s is triggered, the coin goes into **deep sleep mode**.

## Code Structure
The code is structured in 4 parts:
* `bas`: contains ADC boilerplate code and GATT Battery Service
* `io`: contains LED (blinking) and Button handling
* `spaceauth`: registers settings handler for loading the **SPACEKEY** and the **custom GATT Spaceauth Service** that uses the [BLAKE2s hash function](https://blake2.net/) to implement a challenge-response authentication
* `main`: handles the connection and power management while (obviously) containing the main function
