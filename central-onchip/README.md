# Central firmware
This directory contains the firmware that creates the **BLE central** with a USB serial shell interface.
## Command line interface
When opening the serial interface, just press `enter` and you should be the command prompt.

Following main commands are available:
* `central_setup <addr> <irk>`: initial setup of the central with random BLE address and IRK
* `coin add <addr> <irk> <ltk> <spacekey>`: add a new coin (peripheral); you can paste a coin line here
* `coin del <addr>`: delete a registered coin
* `ble_start`: load settings, start BLE stack and begin scanning for peripherals

In addition to that, there are some complementary commands:
* `stats bonds`: prints BLE bonds
* `stats spacekey`: prints registered spacekeys
* `reboot`
* `settings load`: load all settings from storage
* `settings clear`: clear storage (requires reboot)

## Usage
After setup (central and peripheral data is saved automatically to storage), you can start the central using `ble_start`.

Output of a successful authentication could look like this:
```cpp
[00:00:24.987,884] <inf> app: Device found: [d9:b7:fd:4b:c3:54] (RSSI -46) (TYPE 0) (BONDED 1)
[00:00:24.987,915] <inf> app: Battery Level: 39%
[00:00:25.200,714] <inf> app: Connected: [d9:b7:fd:4b:c3:54]
[00:00:25.952,789] <inf> app: Discover complete
[00:00:26.452,789] <inf> app: Coin notified that response is ready.
[00:00:26.652,893] <inf> app: KEY AUTHENTICATED. OPEN DOOR PLEASE.
[00:00:26.753,692] <inf> app: Disconnected: [d9:b7:fd:4b:c3:54] (reason 22)
```

## Code Structure
The code is structured in 3 parts:
* `helper`: constains parsing helper functions and most commands
* `spaceauth`: contains spacekey settings handler, spacekey management functions and the response validation code
* `main`: contains connection management, GATT service discovery, the app statemachine and the `ble_start` command

## App Statemachine
![](https://i.imgur.com/IQAX2zw.png)

