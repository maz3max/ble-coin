#!/usr/bin/python3
import secrets
import re
import fcntl
import os
import binascii
from intelhex import IntelHex


# generate C-Style definition of a byte string
def byte_str_to_c_def(s):
    hex_arr = ["0x%02x" % b for b in s]
    return "{ " + ", ".join(hex_arr) + " }"


# generate human-readable colon-separated BLE address string
def addr_to_str(addr):
    hex_arr = ["%02X" % b for b in addr[::-1]]
    return ":".join(hex_arr)


# generate C-Style definition of an BLE address struct (zephyr-specific)
def addr_to_c_def(addr, addr_type="random"):
    t = "BT_ADDR_LE_RANDOM" if addr_type == "random" else "BT_ADDR_LE_PUBLIC"
    a_str = byte_str_to_c_def(addr)
    return "{ .type = %s, .a = %s }" % (t, a_str)


# generate a full coin line for coins.txt containing address, IRK, LTK and SPACEKEY
def coin_line(periph_addr, periph_irk, ltk, spacekey):
    return "%s %s %s %s\n" % (
        addr_to_str(periph_addr), periph_irk.hex().upper(),  # ID of coin
        ltk.hex().upper(),  # LTK
        spacekey.hex().upper())  # spacekey


# generate contents of main.h including all config data for a coin
def periph_defines(periph_addr, periph_irk, central_addr, central_irk, ltk,
                   spacekey):
    string = "#define INSERT_CENTRAL_ADDR_HERE %s\n" % addr_to_c_def(central_addr)
    string += "#define INSERT_CENTRAL_IRK_HERE %s\n" % byte_str_to_c_def(central_irk)
    string += "#define INSERT_PERIPH_ADDR_HERE %s\n" % addr_to_c_def(periph_addr)
    string += "#define INSERT_PERIPH_IRK_HERE %s\n" % byte_str_to_c_def(periph_irk)
    string += "#define INSERT_LTK_HERE %s\n" % byte_str_to_c_def(ltk)
    string += "#define INSERT_SPACEKEY_HERE %s\n" % byte_str_to_c_def(spacekey)
    return string


# CRC-8-CCITT with initial value 0xFF: checksum used in FCB
def fcb_crc8(data):
    crc8_ccitt_small_table = bytes([0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
                                    0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d])
    val = 0xFF
    for b in data:
        val ^= b
        val = ((val << 4) & 0xFF) ^ crc8_ccitt_small_table[val >> 4]
        val = ((val << 4) & 0xFF) ^ crc8_ccitt_small_table[val >> 4]
    return val


# generate FCB storage item from data
def gen_storage_item(data):
    assert len(data) < 0x4000
    if len(data) < 0x80:
        data_w_len = bytes([len(data)]) + data
    else:
        data_w_len = bytes([(len(data) & 0x7f) | 0x80, len(data) >> 7]) + data
    return data_w_len + bytes([fcb_crc8(data_w_len)])


# generate storage partition
def periph_storage_partition(periph_addr, periph_irk, central_addr, central_irk, ltk,
                             spacekey):
    magic_header = b'\xee\xee\xff\xc0\x01\xff\x00\x00'
    bt_id = b'bt/id=\x01' + bytes(periph_addr)
    bt_irk = b'bt/irk=' + bytes(periph_irk)
    bt_keys = b'bt/keys/' + binascii.hexlify(central_addr[::-1]) + b'1=\x10\x11"\x00' + b'\x00' * 10 + \
              bytes(ltk) + bytes(central_irk) + b'\x00' * 6
    space_key = b'space/key=' + bytes(spacekey)
    data = magic_header + \
           gen_storage_item(bt_id) + \
           gen_storage_item(bt_irk) + \
           gen_storage_item(bt_keys) + \
           gen_storage_item(space_key)
    return data + b'\xff' * (0x6000 - len(data))


def central_storage_partition(central_addr, central_irk):
    magic_header = b'\xee\xee\xff\xc0\x01\xff\x00\x00'
    bt_id = b'bt/id=\x01' + bytes(central_addr)
    bt_irk = b'bt/irk=' + bytes(central_irk)
    data = magic_header + \
           gen_storage_item(bt_id) + \
           gen_storage_item(bt_irk)
    return data + b'\xff' * (0x4000 - len(data))


def prepare_central_hex(central_addr, central_irk, path="central.hex"):
    dir = os.path.dirname(path)
    addr_string = binascii.hexlify(central_addr[::-1]).decode()
    file_name = os.path.join(dir, "central_%s.hex" % addr_string)
    if os.path.exists(file_name):
        return
    storage_bytes = central_storage_partition(central_addr, central_irk)
    storage = IntelHex()
    storage[0xcc000:0xd0000] = list(storage_bytes)
    central = IntelHex(path)
    central.merge(storage, overlap="replace")
    central.tofile(file_name, format="hex")


# regex to parse address and IRK
id_regex = r"^((?:[0-9a-fA-F]{2}\:){5}[0-9a-fA-F]{2})\s*(random|public){0,1}\s*([0-9a-fA-F]{32})"


# parses a line of either the central.txt or coins.txt and extracts BLE address, address type and IRK
def parse_id_line(line):
    m = re.search(id_regex, line)
    if m:
        a = m.group(1)
        hex_arr = a.split(":")
        addr = bytes([int(b, 16) for b in hex_arr[::-1]])
        addr_type = m.group(2)
        if not addr_type:
            addr_type = "random"
        return addr, addr_type, binascii.unhexlify(m.group(3))
    else:
        raise ValueError("Could not parse line")


# reads existing ids from coins.txt into a list
def read_ids(path="coins.txt"):
    result = []
    if not os.path.exists(path):
        return result
    with open(path, "r") as f:
        fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        for line in f:
            addr, foo, bar = parse_id_line(line)
            result.append(addr)
    return result


# appends a coin line to coins.txt (using flock for exclusive access)
def append_id(line, path="coins.txt"):
    with open(path, "a") as f:
        fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        f.write(line)


# reads central address and IRK if central.txt exists, otherwise initializes it
def gen_central(path="central.txt"):
    if os.path.exists(path):
        with open(path, "r") as f:
            return parse_id_line(f.readline())

    else:
        central_addr = bytearray(secrets.token_bytes(6))
        central_addr[5] |= 0xc0
        central_addr = bytes(central_addr)
        central_irk = secrets.token_bytes(16)
        with open(path, "w") as f:
            f.write("%s %s" % (addr_to_str(central_addr), central_irk.hex().upper()))
        return central_addr, "random", central_irk


# generates new coin info making sure it has a unique address
def gen_peripheral(addr_list):
    peripheral_addr = bytearray(secrets.token_bytes(6))
    peripheral_addr[5] |= 0xc0
    peripheral_addr = bytes(peripheral_addr)
    while peripheral_addr in addr_list:
        peripheral_addr = bytearray(secrets.token_bytes(6))
        peripheral_addr[5] |= 0xc0
        peripheral_addr = bytes(peripheral_addr)
    peripheral_irk = secrets.token_bytes(16)
    return peripheral_addr, peripheral_irk


if __name__ == '__main__':
    # prepare IDs
    p_addr, p_irk = gen_peripheral(read_ids())
    c_addr, c_addr_type, c_irk = gen_central()

    print("Central: " + addr_to_str(c_addr))
    print("Peripheral: " + addr_to_str(p_addr))

    # prepare keys
    ltk = secrets.token_bytes(16)
    spacekey = secrets.token_bytes(32)

    # write coin line
    append_id(coin_line(p_addr, p_irk, ltk, spacekey))

    # create defines file
    # with open("../factory-bonding-onchip/src/main.h", "w") as f:
    #     f.write(periph_defines(p_addr, p_irk, c_addr, c_irk, ltk, spacekey))

    # create storage partition
    storage_bytes = periph_storage_partition(p_addr, p_irk, c_addr, c_irk, ltk, spacekey)

    addr_string = binascii.hexlify(p_addr[::-1]).decode()

    # create merged hex file for easy programming
    storage = IntelHex()
    storage[0x32000:0x38000] = list(storage_bytes)
    # storage.tofile("storage_%s.hex" % addr_string, format="hex")
    coin = IntelHex("coin.hex")
    coin.merge(storage, overlap="replace")
    coin[0x10001208:0x10001208 + 4] = [0x00] * 4  # enable Access Port Protection
    coin.tofile("coin_%s.hex" % addr_string, format="hex")

    prepare_central_hex(c_addr, c_irk)
