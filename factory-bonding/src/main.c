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
#include <keys.h>
#include <bsd/stdlib.h>
#include <stdio.h>
#include <regex.h>

#define CHECK_ERR if (err) { printk("ERROR %i\n", err); return; }

#define BLAKE2S_KEYBYTES    32

bt_addr_le_t get_random_address() {
    bt_addr_le_t a = {.type=BT_ADDR_LE_RANDOM, .a={0}};
    arc4random_buf(a.a.val, 6);
    BT_ADDR_SET_STATIC(&a.a);
    return a;
}

bool deserialize_id(FILE *fp, bt_addr_le_t *id,
                    struct bt_irk *id_irk) {
    bool result = true;

    regex_t rx_id, rx_irk;
    regcomp(&rx_id, "ID: (([[:xdigit:]]{2}\\:){5}[[:xdigit:]]{2}) (random|public)", REG_EXTENDED);
    regcomp(&rx_irk, "IRK: ([[:xdigit:]]{32})", REG_EXTENDED);

    regmatch_t matches[8];
    char *line = NULL;
    size_t len = 0;
    if (getline(&line, &len, fp) > 0 && regexec(&rx_id, line, 8, matches, 0) == 0 && matches[0].rm_so != -1) {
        if (!strncmp("random", line + matches[3].rm_so, 6)) {
            id->type = BT_ADDR_LE_RANDOM;
        } else if (!strncmp("public\n", line + matches[3].rm_so, 6)) {
            id->type = BT_ADDR_LE_PUBLIC;
        }
        for (int i = 0; i < 6; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 3)], line[matches[1].rm_so + (i * 3) + 1], 0};
            id->a.val[5 - i] = strtol(buf, NULL, 16);
        }
    } else {
        result = false;
    }
    if (getline(&line, &len, fp) > 0 && regexec(&rx_irk, line, 8, matches, 0) == 0 && matches[0].rm_so != -1) {
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 2)], line[matches[1].rm_so + (i * 2) + 1], 0};
            id_irk->val[i] = strtol(buf, NULL, 16);
        }
    } else {
        result = false;
    }
    regfree(&rx_id);
    regfree(&rx_irk);
    return result;
}

bool deserialize_keys(FILE *fp, struct bt_keys *keys, uint8_t *spacekey) {
    regex_t rx_ltk, rx_l_csrk, rx_r_csrk, rx_spacekey;
    regex_t *regexes[] = {&rx_ltk, &rx_l_csrk, &rx_r_csrk, &rx_spacekey};
    const char *regex_str[] = {
            "LTK: ([[:xdigit:]]{32})",
            "LOCAL_CSRK: ([[:xdigit:]]{32}) CNT: (\\d+)",
            "REMOTE_CSRK: ([[:xdigit:]]{32}) CNT: (\\d+)",
            "SPACE_KEY: ([[:xdigit:]]{64})"};
    for (size_t i = 0; i < 4; ++i) {
        if (regcomp(regexes[i], regex_str[i], REG_EXTENDED)) {
            fprintf(stderr, "Error compiling regex %i\n", i);
        }
    }

    bool result = false;

    regmatch_t matches[8];
    char *line = NULL;
    size_t len = 0;
    if (getline(&line, &len, fp) > 0 && regexec(&rx_ltk, line, 8, matches, 0) == 0 && matches[0].rm_so != -1) {
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 2)], line[matches[1].rm_so + (i * 2) + 1], 0};
            keys->ltk.val[i] = strtol(buf, NULL, 16);
        }
    } else {
        result = false;
    }
    if (getline(&line, &len, fp) > 0 && regexec(&rx_l_csrk, line, 8, matches, 0) == 0 && matches[0].rm_so != -1) {
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 2)], line[matches[1].rm_so + (i * 2) + 1], 0};
            keys->local_csrk.val[i] = strtol(buf, NULL, 16);
        }
        char buf[12];
        strcpy(buf, line + matches[2].rm_so);
        keys->local_csrk.cnt = strtol(buf, NULL, 10);
    } else {
        result = false;
    }
    if (getline(&line, &len, fp) > 0 && regexec(&rx_r_csrk, line, 8, matches, 0) == 0 && matches[0].rm_so != -1) {
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 2)], line[matches[1].rm_so + (i * 2) + 1], 0};
            keys->remote_csrk.val[i] = strtol(buf, NULL, 16);
        }
        char buf[12];
        strcpy(buf, line + matches[2].rm_so);
        keys->remote_csrk.cnt = strtol(buf, NULL, 10);
    } else {
        result = false;
    }
    if (getline(&line, &len, fp) > 0 && regexec(&rx_spacekey, line, 8, matches, 0) == 0 && matches[0].rm_so != -1) {
        for (int i = 0; i < 32; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 2)], line[matches[1].rm_so + (i * 2) + 1], 0};
            spacekey[i] = strtol(buf, NULL, 16);
        }
    } else {
        result = false;
    }
    keys->enc_size = BT_ENC_KEY_SIZE_MAX;
    keys->flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC);
    keys->keys = (BT_KEYS_IRK | BT_KEYS_LOCAL_CSRK | BT_KEYS_REMOTE_CSRK | BT_KEYS_LTK_P256);
    keys->id = BT_ID_DEFAULT;
    regfree(&rx_ltk);
    regfree(&rx_l_csrk);
    regfree(&rx_r_csrk);
    regfree(&rx_spacekey);
    return result;
}


// save identity into file
void serialize_id(const bt_addr_le_t *id,
                  const struct bt_irk *id_irk,
                  FILE *fp) {
    const char *type = (id->type == BT_ADDR_LE_PUBLIC) ? "public" : "random";
    fprintf(fp, "ID: %02X:%02X:%02X:%02X:%02X:%02X %s\n",
            id->a.val[5], id->a.val[4], id->a.val[3],
            id->a.val[2], id->a.val[1], id->a.val[0], type);
    fprintf(fp, "IRK: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
            id_irk->val[0], id_irk->val[1], id_irk->val[2], id_irk->val[3],
            id_irk->val[4], id_irk->val[5], id_irk->val[6], id_irk->val[7],
            id_irk->val[8], id_irk->val[9], id_irk->val[10], id_irk->val[11],
            id_irk->val[12], id_irk->val[13], id_irk->val[14], id_irk->val[15]);
}

// save keychain needed to connect to peripheral into file
void serialize_keys(const struct bt_keys *keys, const uint8_t *spacekey,
                    FILE *fp) {
    fprintf(fp, "LTK: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
            keys->ltk.val[0], keys->ltk.val[1], keys->ltk.val[2], keys->ltk.val[3],
            keys->ltk.val[4], keys->ltk.val[5], keys->ltk.val[6], keys->ltk.val[7],
            keys->ltk.val[8], keys->ltk.val[9], keys->ltk.val[10], keys->ltk.val[11],
            keys->ltk.val[12], keys->ltk.val[13], keys->ltk.val[14], keys->ltk.val[15]);
    fprintf(fp, "LOCAL_CSRK: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X CNT: %u\n",
            keys->local_csrk.val[0], keys->local_csrk.val[1], keys->local_csrk.val[2], keys->local_csrk.val[3],
            keys->local_csrk.val[4], keys->local_csrk.val[5], keys->local_csrk.val[6], keys->local_csrk.val[7],
            keys->local_csrk.val[8], keys->local_csrk.val[9], keys->local_csrk.val[10], keys->local_csrk.val[11],
            keys->local_csrk.val[12], keys->local_csrk.val[13], keys->local_csrk.val[14], keys->local_csrk.val[15],
            keys->local_csrk.cnt);
    fprintf(fp, "REMOTE_CSRK: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X CNT: %u\n",
            keys->remote_csrk.val[0], keys->remote_csrk.val[1], keys->remote_csrk.val[2], keys->remote_csrk.val[3],
            keys->remote_csrk.val[4], keys->remote_csrk.val[5], keys->remote_csrk.val[6], keys->remote_csrk.val[7],
            keys->remote_csrk.val[8], keys->remote_csrk.val[9], keys->remote_csrk.val[10], keys->remote_csrk.val[11],
            keys->remote_csrk.val[12], keys->remote_csrk.val[13], keys->remote_csrk.val[14], keys->remote_csrk.val[15],
            keys->remote_csrk.cnt);
    fprintf(fp,
            "SPACE_KEY: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
            spacekey[0], spacekey[1], spacekey[2], spacekey[3],
            spacekey[4], spacekey[5], spacekey[6], spacekey[7],
            spacekey[8], spacekey[9], spacekey[10], spacekey[11],
            spacekey[12], spacekey[13], spacekey[14], spacekey[15],
            spacekey[16], spacekey[17], spacekey[18], spacekey[19],
            spacekey[20], spacekey[21], spacekey[22], spacekey[23],
            spacekey[24], spacekey[25], spacekey[26], spacekey[27],
            spacekey[28], spacekey[29], spacekey[30], spacekey[31]);
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
    bt_addr_le_t periph_addr = get_random_address();
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
    FILE *out = fopen("periph_conn.txt", "a+");
    serialize_id(&periph_addr, &periph_irk, out);
    serialize_keys(&central_keys, spacekey, out);
    fclose(out);
    /*
    out = fopen("central_conn.txt", "w");
    serialize_id(&central_addr, &central_irk, out);
    serialize_keys(&periph_keys, spacekey, out);
    fclose(out);
     */
    exit(0);
}
