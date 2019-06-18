/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>


#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/crypto.h>

#include <bsd/stdlib.h>

#include "unistd.h"

#include "guestlist.h"

#define CHECK_ERR if (err) { printk("ERROR %i\n", err); return; }

#define BLAKE2S_KEYBYTES    32

bt_addr_le_t get_random_address() {
    bt_addr_le_t a = {.type=BT_ADDR_LE_RANDOM, .a={0}};
    arc4random_buf(a.a.val, 6);
    BT_ADDR_SET_STATIC(&a.a);
    return a;
}

bool check_if_addr_exists(bt_addr_le_t *addr) {
    struct bt_irk irk;
    bt_addr_le_t id;
    if(access("coins.txt", F_OK) == -1){
        return false;
    }
    FILE *fp = open_guestlist("r");
    bool matched = false;
    while (deserialize_id(fp, &id, &irk)) {
        if (bt_addr_le_cmp(addr, &id) == 0) {
            matched = true;
            break;
        }
    }
    fclose(fp);
    return matched;
}

void main(int argc, char *argv[]) {
    int err;
    bt_addr_le_t central_addr = {0};
    struct bt_irk central_irk = {0};
    FILE *in = fopen("central.txt", "r");
    if (!(in && deserialize_id(in, &central_addr, &central_irk))) {
        printk("HAVE TO generate central info...\n");
        arc4random_buf(central_irk.val, 16);
        central_addr = get_random_address();
        FILE *out = fopen("central.txt", "w");
        serialize_id(&central_addr, &central_irk, out);
        fclose(out);
    } else {
        fclose(in);
    }


    printk("generating periph address...\n");
    bt_addr_le_t periph_addr = {0};//get_random_address();
    while (check_if_addr_exists(&periph_addr)) {
        periph_addr = get_random_address();
    }
    printk("generating periph irk...\n");
    struct bt_irk periph_irk = {0};
    arc4random_buf(periph_irk.val, 16);
    struct bt_keys periph_keys = {
            .id = BT_ID_DEFAULT,
            .addr = central_addr,
            .irk = central_irk,
            .enc_size = BT_ENC_KEY_SIZE_MAX,
            .flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC),
            .keys = (BT_KEYS_IRK | BT_KEYS_LOCAL_CSRK | BT_KEYS_REMOTE_CSRK | BT_KEYS_LTK_P256),
            .ltk = {0},
            .local_csrk = {0},
            .remote_csrk = {0},
    };
    arc4random_buf(periph_keys.ltk.val, 16);
    arc4random_buf(periph_keys.irk.val, 16);
    arc4random_buf(periph_keys.local_csrk.val, 16);
    arc4random_buf(periph_keys.remote_csrk.val, 16);
    struct bt_keys central_keys = {
            .id = BT_ID_DEFAULT,
            .addr = periph_addr,
            .irk = periph_irk,
            .enc_size = BT_ENC_KEY_SIZE_MAX,
            .flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC),
            .keys = (BT_KEYS_IRK | BT_KEYS_LOCAL_CSRK | BT_KEYS_REMOTE_CSRK | BT_KEYS_LTK_P256),
            .ltk = periph_keys.ltk,
            .local_csrk = periph_keys.remote_csrk,
            .remote_csrk = periph_keys.local_csrk,
    };

    uint8_t spacekey[BLAKE2S_KEYBYTES];
    arc4random_buf(spacekey, BLAKE2S_KEYBYTES);

    settings_subsys_init();
    printk("Saving ID\n");
    err = settings_save_one("bt/id", &periph_addr, sizeof(periph_addr));
    CHECK_ERR
    printk("Saving IRK\n");
    err = settings_save_one("bt/irk", &periph_irk, sizeof(periph_irk));
    CHECK_ERR
    printk("Saving KEYS\n");
    err = bt_keys_store(&periph_keys);
    CHECK_ERR
    printk("Saving SpaceKey\n");
    err = settings_save_one("space/key", &spacekey, BLAKE2S_KEYBYTES);
    CHECK_ERR
    settings_commit();
    settings_save();
    printk("Done.\n");

    FILE *out = open_guestlist("a+");
    serialize_keys(&central_keys, spacekey, out);
    fclose(out);
    exit(0);
}
