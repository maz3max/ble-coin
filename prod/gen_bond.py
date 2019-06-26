import secrets
import re
import fcntl
import os
import binascii

id_regex = r"^((?:[0-9a-fA-F]{2}\:){5}[0-9a-fA-F]{2})\s*(random|public)\s*([0-9a-fA-F]{32})"


def byte_str_to_c_def(s):
    hex_arr = ["0x%02x" % b for b in s]
    return "{ " + ", ".join(hex_arr) + " }"


def addr_to_str(addr, addr_type="random"):
    hex_arr = ["%02X" % b for b in addr[::-1]]
    return ":".join(hex_arr) + " " + addr_type


def addr_to_c_def(addr, addr_type="random"):
    t = "BT_ADDR_LE_RANDOM" if addr_type == "random" else "BT_ADDR_LE_PUBLIC"
    a_str = byte_str_to_c_def(addr)
    return "{ .type = %s, .a = %s }" % (t, a_str)


def coin_line(periph_addr, periph_irk, ltk, spacekey):
    return "%s %s %s %s\n" % (
        addr_to_str(periph_addr), periph_irk.hex().upper(),  # ID of coin
        ltk.hex().upper(),  # LTK
        spacekey.hex().upper())  # spacekey


def periph_defines(periph_addr, periph_irk, central_addr, central_irk, ltk,
                   spacekey):
    string = "#define INSERT_CENTRAL_ADDR_HERE %s\n" % addr_to_c_def(central_addr)
    string += "#define INSERT_CENTRAL_IRK_HERE %s\n" % byte_str_to_c_def(central_irk)
    string += "#define INSERT_PERIPH_ADDR_HERE %s\n" % addr_to_c_def(periph_addr)
    string += "#define INSERT_PERIPH_IRK_HERE %s\n" % byte_str_to_c_def(periph_irk)
    string += "#define INSERT_LTK_HERE %s\n" % byte_str_to_c_def(ltk)
    string += "#define INSERT_SPACEKEY_HERE %s\n" % byte_str_to_c_def(spacekey)
    return string


def parse_id_line(line):
    m = re.search(id_regex, line)
    if m:
        a = m.group(1)
        hex_arr = a.split(":")
        addr = bytes([int(b, 16) for b in hex_arr[::-1]])
        addr_type = m.group(2)
        return addr, addr_type, binascii.unhexlify(m.group(3))
    else:
        raise ValueError("Could not parse line")


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


def append_id(line, path="coins.txt"):
    with open(path, "a") as f:
        fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        f.write(line)


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

    print("Central: " + addr_to_str(c_addr, c_addr_type))
    print("Peripheral: " + addr_to_str(p_addr))

    # prepare keys
    ltk = secrets.token_bytes(16)
    spacekey = secrets.token_bytes(32)

    # write coin line
    append_id(coin_line(p_addr, p_irk, ltk, spacekey))

    # create defines file
    with open("../factory-bonding-onchip/src/main.h", "w") as f:
        f.write(periph_defines(p_addr, p_irk, c_addr, c_irk, ltk, spacekey))
