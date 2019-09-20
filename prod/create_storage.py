#!/usr/bin/python3
from intelhex import IntelHex as IH

storage_mapping = {
    'nrf52_pca10040': 0x7a000,
    'nrf52_pca20020': 0x7a000,
    'nrf52810_pca10040': 0x29000,
    'nrf52840_pca10059-stock': 0xcc000,
    'nrf52840_pca10059-debugger': 0xcc000,
    'nrf52840_pca10056': 0xf8000
}

if __name__ == '__main__':
    with open('central_storage_s.bin', 'rb') as file:
        central = file.read()
    with open('peripheral_storage_s.bin', 'rb') as file:
        peripheral = file.read()
    for k, v in storage_mapping.items():
        c = IH()
        c[v:v + 1024] = list(central)
        fname = k + '_' + str(hex(v)) + '.hex'
        c.tofile('central_' + fname, format='hex')
        p = IH()
        p[v:v + 1024] = list(peripheral)
        p.tofile('peripheral_' + fname, format='hex')

