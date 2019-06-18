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

#define BLAKE2S_KEYBYTES    32

bt_addr_le_t get_random_address() {
    bt_addr_le_t a = {.type=BT_ADDR_LE_RANDOM, .a={0}};
    arc4random_buf(a.a.val, 6);
    BT_ADDR_SET_STATIC(&a.a);
    return a;
}

static FILE *gl = NULL;

void initialize(void) {
    FILE *c = fopen("central.txt", "r");
    struct bt_irk id_irk;
    bt_addr_le_t id;
    if (c && deserialize_id(c, &id, &id_irk)) {
        bt_id_create(&id, id_irk.val);
    } else {
        fprintf(stderr, "Couldn't load central ID!\n");
        exit(-1);
    }
    fclose(c);

    gl = open_guestlist("r");
    struct bt_keys keys;
    uint8_t spacekey[BLAKE2S_KEYBYTES];
    while (gl && deserialize_keys(gl, &keys, spacekey)) {
        space_add_id(&keys, spacekey);
    }
    bt_keys_foreach(BT_KEYS_ALL, space_save_id, stdout);
}

void finalize(void) {
    gl = freopen(NULL, "w+", gl);
    bt_keys_foreach(BT_KEYS_ALL, space_save_id, gl);
    fclose(gl);
}


NATIVE_TASK(finalize, ON_EXIT, 0);

void main(int argc, char *argv[]) {
    initialize();
}
