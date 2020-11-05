#!/usr/bin/python3
import re
import aioserial
import asyncio
import multiprocessing
import serial.serialutil
import os
import sys
import time
from enum import IntEnum


class KeykeeperDB:
    def __init__(self):
        self.coins = {}
        self.identity = []
        self.load()

    def load(self, coins="coins.txt", central="central.txt"):
        coin_list = []
        identity = None
        with open(coins, "r") as f:
            for line in f:
                m = re.match(r"(.{17})\s+(.{32})\s+(.{32})\s+(.{64})", line)
                if m:
                    self.coins[m.group(1)] = m.groups()[1:]
        with open(central, "r") as f:
            line = f.readline()
            m = re.match(r"(.{17})\s+(.{32})", line)
            if m:
                self.identity = m.groups()


class StatusType(IntEnum):
    IDENTITY = 0
    DEVICE_FOUND = 1
    BATTERY_LEVEL = 2
    CONNECTED = 3
    AUTHENTICATED = 4
    DISCONNECTED = 5


class Coin:
    def __init__(self):
        self.battery_level = 0
        self.address = "00:00:00:00:00:00"


class KeykeeperSerialMgr:
    def __init__(self, db, status_pipe):
        self.config_mode = True
        self.db = db
        self.status_pipe = status_pipe

    # read line and remove color codes
    async def _serial_fetch_line(self):
        line = (await self.central_serial.readline_async()).decode(errors='ignore')
        plain_line = re.sub(r'''
            \x1B    # ESC
            [@-_]   # 7-bit C1 Fe
            [0-?]*  # Parameter bytes
            [ -/]*  # Intermediate bytes
            [@-~]   # Final byte
        ''', '', line, flags=re.VERBOSE)
        return plain_line

    # parse status messages
    def _parse_status(self, l):
        regs = {
            StatusType.IDENTITY: r"<inf> bt_hci_core: Identity: (.{17}) \((.*)\)",
            StatusType.DEVICE_FOUND: r"<inf> app: Device found: \[(.{17})\] \(RSSI (-?\d+)\) \(TYPE (\d)\) \(BONDED (\d)\)",
            StatusType.BATTERY_LEVEL: r"<inf> app: Battery Level: (\d{1,3})%",
            StatusType.CONNECTED: r"<inf> app: Connected: \[(.{17})\]",
            StatusType.AUTHENTICATED: r"<inf> app: KEY AUTHENTICATED. OPEN DOOR PLEASE.",
            StatusType.DISCONNECTED: r"<inf> app: Disconnected: \[(.{17})\] \(reason (\d+)\)",
        }
        for k in regs:
            m = re.search(pattern=regs[k], string=l)
            if m:
                return k, m.groups()
        return None, None

    # read registered bonds
    async def _request_bonds(self):
        bonds = []
        self.central_serial.write(b'stats bonds\r\n')
        line = None
        while not (line and line.endswith('stats bonds\r\n')):
            line = await self._serial_fetch_line()
            # print(line, end='', flush=True)
        while line != 'done\r\n':
            line = await self._serial_fetch_line()
            bond = re.match(r"\[(.{17})\] keys: 34, flags: 17\r\n", line)
            if bond:
                bonds.append(bond.groups())
        return bonds

    # read registered spacekeys (only first byte)
    async def _request_spacekeys(self):
        spacekeys = []
        self.central_serial.write(b'stats spacekey\r\n')
        line = None
        while not (line and line.endswith('stats spacekey\r\n')):
            line = await self._serial_fetch_line()
            # print(line, end='', flush=True)
        while line != 'done\r\n':
            line = await self._serial_fetch_line()
            spacekey = re.match(r"\[(.{17})\] : ([A-F0-9]{2})\.\.\.\r\n", line)
            if spacekey:
                spacekeys.append(spacekey.groups())
        return spacekeys

    # read settings
    async def _read_settings(self):
        self.central_serial.write(b'settings load\r\n')
        line = None
        k = None
        while k != StatusType.IDENTITY:
            line = await self._serial_fetch_line()
            # print(line, end='', flush=True)
            k, v = self._parse_status(line)
            if 'bt_hci_core: Read Static Addresses command not available' in line:
                break
            if k == StatusType.IDENTITY:
                self.identity = v[0].upper()

    async def _wait_until_done(self):
        line = None
        while line != 'done\r\n':
            line = await self._serial_fetch_line()
            # print(line, end='', flush=True)

    # main state machine routine

    async def _manage_serial(self):
        # clear old state
        self.identity = None
        self.bonds = None
        self.spacekeys = None

        if self.config_mode:
            os.write(self.status_pipe, str(
                "status: synchronizing database").encode('utf8'))
            # just load settings, don't start scanning
            await self._read_settings()
            # read coin data from device
            self.bonds = await self._request_bonds()
            self.spacekeys = await self._request_spacekeys()
            if self.identity != self.db.identity[0]:
                if self.identity:
                    self.central_serial.write(b'settings clear\r\n')
                    await self._wait_until_done()
                else:
                    self.central_serial.write('central_setup {} {}\r\n'.format(
                        *self.db.identity).encode('ASCII'))
                    await self._wait_until_done()
            if len(self.bonds) != len(self.spacekeys):
                self.central_serial.write(b'settings clear\r\n')
            is_present = {c: False for c in self.db.coins.keys()}
            for bond, skey in zip(self.bonds, self.spacekeys):
                if bond[0] != skey[0]:
                    self.central_serial.write(b'settings clear\r\n')
                    await self._wait_until_done()
                if bond[0] not in self.db.coins or skey[1] != self.db.coins[bond[0]][2][:2]:
                    self.central_serial.write(
                        'coin del {}\r\n'.format(bond[0]).encode('ASCII'))
                    await self._wait_until_done()
                else:
                    is_present[bond[0]] = True
            for addr, present in is_present.items():
                if not present:
                    self.central_serial.write('coin add {} {} {} {}\r\n'.format(
                        addr, *self.db.coins[addr]).encode('ASCII'))
                    await self._wait_until_done()
            self.config_mode = False
            self.central_serial.write(b'reboot\r\n')
            await self._wait_until_done()
        else:
            # start BLE stack
            self.central_serial.write(b'ble_start\r\n')
            os.write(self.status_pipe, str(
                "status: central connected and scanning").encode('utf8'))

        # main event loop
        while True:
            line = await self._serial_fetch_line()
            # print(line, end='', flush=True)
            k, v = self._parse_status(line)
            if k == StatusType.IDENTITY:
                self.identity = v[0].upper()
            if k == StatusType.AUTHENTICATED:
                os.write(self.status_pipe, str("status: {} ({}%🔋) authenticated".format(
                    self.current_coin.address, self.current_coin.battery_level)).encode('utf8'))
            elif k == StatusType.BATTERY_LEVEL:
                self.current_coin.battery_level = v[0]
            elif k == StatusType.CONNECTED:
                self.current_coin.address = v[0].upper()
            elif k == StatusType.DISCONNECTED:
                self.current_coin = Coin()

    # main loop with reconnecting
    async def run_async(self):
        self.current_coin = Coin()

        first_start = True
        while True:
            try:
                self.central_serial = aioserial.AioSerial(
                    port=os.path.realpath('/dev/serial/by-id/usb-ZEPHYR_N39_BLE_KEYKEEPER_0.01-if00'))
                self.central_serial.write(b'\r\n\r\n')
                if first_start:
                    self.central_serial.write(b'reboot\r\n')
                    first_start = False
                    await self._wait_until_done()

                else:
                    await self._manage_serial()
            except serial.serialutil.SerialException:
                os.write(self.status_pipe, str(
                    "status: connecting to central").encode('utf8'))
                await asyncio.sleep(1)

    def run(self):
        asyncio.run(self.run_async())


def _run_serialmgr():
    db = KeykeeperDB()
    pipein, pipeout = os.pipe()
    k = KeykeeperSerialMgr(db, pipeout)
    p = multiprocessing.Process(target=k.run, daemon=True)
    p.start()
    while True:
        print(os.read(pipein, 100).decode('utf8'))


if __name__ == "__main__":
    _run_serialmgr()
