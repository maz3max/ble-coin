#!/usr/bin/python3
import argparse
import binascii
from textwrap import wrap
from intelhex import IntelHex as IH

parser = argparse.ArgumentParser(description='Analyze Zephyr FCB storage and print contents.')
parser.add_argument('file', help='binary dump of the storage partition')


def fcb_crc8(data):
    crc8_ccitt_small_table = bytes([0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
                                    0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d])
    val = 0xFF
    for b in data:
        val ^= b
        val = ((val << 4) & 0xFF) ^ crc8_ccitt_small_table[val >> 4]
        val = ((val << 4) & 0xFF) ^ crc8_ccitt_small_table[val >> 4]
    return val


def read_items(storage):
    items = []
    assert storage[:4] == b'\xee\xee\xff\xc0', 'no magic sequence detected!'
    print('FCB version: %u' % storage[4])
    assert storage[5] == 0xFF, 'padding not detected'
    print('FD ID: %u' % (storage[6] + storage[7] << 8))
    off = 8
    while off < len(storage) and storage[off] != 0xFF:
        if storage[off] & 0x80:
            length = (storage[off] & 0x7f) | (storage[off + 1] << 7)
            data_end = off + 2 + length
        else:
            length = storage[off]
            data_end = off + 1 + length
        data = storage[off + 1: data_end]
        crc = storage[data_end]
        if crc == fcb_crc8(storage[off:data_end]):
            items.append(data)
        else:
            print('CRC check failed!')
        off = data_end + 1
    return items


def read_setting(item):
    if len(item) == 13 and item[:6] == b'bt/id=':
        print('bt/id:', end=' ')
        id_type = item[6]
        id = item[12:6:-1]  # addr is reversed
        print(':'.join(['%02X' % x for x in id]), 'type=' + str(id_type))
    elif len(item) == 23 and item[:7] == b'bt/irk=':
        print('bt/irk:', end=' ')
        periph_irk = item[7:23]
        print(binascii.hexlify(periph_irk).decode().upper())
    elif len(item) == 74 and item[:8] == b'bt/keys/':
        print('bt/keys:', end=' ')
        print(':'.join(wrap(item[8:20].decode().upper(), 2)), 'type=' + bytes([item[20]]).decode(), end=' ')
        assert item[21] == b'='[0]
        print('enc_size=%u' % item[22], end=' ')
        print('flags=%s' % bin(item[23]), end=' ')
        print('keys=%s' % bin(item[24]), end=' ')
        assert item[25] == b'\x00'[0]  # padding[1]?
        rand = item[26:34]
        if rand != b'\x00' * 8:
            print('RAND=%s' % binascii.hexlify(rand).decode().upper(), end=' ')
        ediv = item[34:36]
        if ediv != b'\x00' * 2:
            print('EDIV=%s' % binascii.hexlify(ediv).decode().upper(), end=' ')
        ltk = item[36:52]
        print('LTK=%s' % binascii.hexlify(ltk).decode().upper(), end=' ')
        central_irk = item[52:68]
        print('IRK=%s' % binascii.hexlify(central_irk).decode().upper(), end=' ')
        rpa = item[73:67:-1]  # rpa[6], reversed address
        if rpa != b'\x00' * 6:
            print('RPA=', end='')
            print(':'.join(['%02X' % x for x in rpa]), end='')
        print('')
    elif len(item) == 42 and item[:10] == b'space/key=':
        print('space/key:', end=' ')
        spacekey = item[10:42]
        print(binascii.hexlify(spacekey).decode().upper())
    elif len(item) == 52 and item[:6] == b'space/':
        print('space:', end=' ')
        print(':'.join(wrap(item[6:18].decode().upper(), 2)), 'type=' + bytes([item[18]]).decode(), end=' ')
        assert item[19] == b'='[0]
        print('spacekey=%s' % binascii.hexlify(item[20:52]).decode().upper())
    else:
        print(item)


if __name__ == '__main__':
    args = parser.parse_args()
    if args.file[-4:] == '.bin':
        with open(args.file, "rb") as file:
            storage = file.read()
    elif args.file[-4:] == '.hex':
        ih = IH(args.file)
        storage = ih.tobinstr()
    else:
        print("unrecognized file extension", file=sys.stderr)
        sys.exit(-1)
    items = read_items(storage)
    for i in items:
        read_setting(i)
