/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>


#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/crypto.h>
#include <bsd/stdlib.h>

#include "kernel.h"
#include "soc.h"

#include "spaceauth.h"

#include "guestlist.h"

#define BLAKE2S_KEYBYTES    32

bt_addr_le_t get_random_address() {
    bt_addr_le_t a = {.type=BT_ADDR_LE_RANDOM, .a={0}};
    arc4random_buf(a.a.val, 6);
    BT_ADDR_SET_STATIC(&a.a);
    return a;
}

// NATIVE_TASK(settings_save, ON_EXIT, 0);

void main(int argc, char *argv[]) {
    bt_addr_le_t addr = {0};
    struct bt_irk irk = {0};
    uint8_t key[BLAKE2S_KEYBYTES];
    struct bt_keys keys = {0};

    FILE *in = open_guestlist();
    if (!in) {
        exit(-1);
    }
    deserialize_id(in, &addr, &irk);
    serialize_id(&addr, &irk, stdout);
    rewind(in);
    deserialize_keys(in, &keys, key);
    serialize_keys(&keys, key, stdout);
    fclose(in);
    exit(0);
}
