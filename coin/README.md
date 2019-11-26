# Coin Firmware
This directory contains the firmware that creates a **BLE peripheral** loading the **BLE bond** and the **SPACEKEY** from flash using zephyr's settings API.

## Usage
Please refer to the top level README file for flashing instructions. Especially, this firmware will **not** work properly when flashed onto a virgin, mass-erased chip, because the **settings partition** needs to be flashed too.

Flashing can be done [using a BlackMagic Probe](https://github.com/blacksphere/blackmagic/wiki) or [using an STLinkV2 with OpenOCD](https://medium.com/@ly.lee/coding-nrf52-with-rust-and-apache-mynewt-on-visual-studio-code-9521bcba6004).

Please note that you will not be able to lift the **Access Port Protection** with an STLinkV2. That means the coins are **locked down** from flashing until you get another flash tool. Don't worry - some STLinkV2 clones can be turned into BlackMagic Probes if they have sufficient (128K) flash. You could also buy a real BlackMagic Probe, an FT2232H breakout board or a SEGGER JLink. It is possible to [remove the nRF52 Flash Protection with a Raspberry Pi](https://medium.com/@ly.lee/coding-nrf52-with-rust-and-apache-mynewt-on-visual-studio-code-9521bcba6004).

When flashed, press the button on the coin to wake it up. The LED will light up and the coin will send **BLE advertisements** to the central signaling that it wants to connect. The LED starts flashing when the coin finds its central.

When finished authenticating, on connection loss or when a timeout of 10s is triggered, the coin goes into **deep sleep mode**.

## Code Structure
The code is structured in 4 parts:
* `bas`: contains ADC boilerplate code and GATT Battery Service
* `io`: contains LED (blinking) and Button handling
* `spaceauth`: registers settings handler for loading the **SPACEKEY** and the **custom GATT Spaceauth Service** that uses the [BLAKE2s hash function](https://blake2.net/) to implement a challenge-response authentication
* `main`: handles the connection and power management while (obviously) containing the main function
