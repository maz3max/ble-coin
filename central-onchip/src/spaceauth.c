#include <stdlib.h>
#include <ctype.h>
#include "spaceauth.h"
#include <settings/settings.h>
#include <tinycrypt/utils.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(space);


#include "blake2.h"


#ifndef CONFIG_BT_MAX_PAIRED
#define CONFIG_BT_MAX_PAIRED -1
#endif

static spacekey_t keys[CONFIG_BT_MAX_PAIRED] = {0};
static const bt_addr_t NO_ADDR = {0};
static size_t largest_index_used = 0;

static void space_settings_encode_key(char *path, size_t path_size, const bt_addr_le_t *addr) {
    assert(path_size > 13)
    snprintk(path, path_size, "space/%02x%02x%02x%02x%02x%02x%u",
             addr->a.val[5], addr->a.val[4], addr->a.val[3],
             addr->a.val[2], addr->a.val[1], addr->a.val[0],
             addr->type);
}


static int space_settings_decode_key(const char *key, int key_len, bt_addr_le_t *addr) {
    if (key_len != 13) {
        return -EINVAL;
    }
    for (size_t i = 0; i < 6; ++i) {
        char buf[] = {key[2 * i], key[2 * i + 1], 0};
        if (isxdigit(buf[0]) && isxdigit(buf[1])) {
            addr->a.val[5 - i] = strtol(buf, NULL, 16);
        } else {
            LOG_ERR("invalid hex string");
            return -EINVAL;
        }
    }
    if (key[12] == '0') {
        addr->type = BT_ADDR_LE_PUBLIC;
    } else if (key[12] == '1') {
        addr->type = BT_ADDR_LE_RANDOM;
    } else {
        return -EINVAL;
    }
    return 0;
}

static spacekey_t *spacekey_lookup_add(const bt_addr_le_t *addr) {
    spacekey_t *result = spacekey_lookup(addr);
    if (result) {
        return result;
    }
    for (size_t i = 0; i < CONFIG_BT_MAX_PAIRED; ++i) {
        if (!bt_addr_cmp(&NO_ADDR, &keys[i].addr.a)) {
            largest_index_used = MAX(i, largest_index_used);
            return &keys[i];
        }
    }
    return NULL;
}

spacekey_t *spacekey_lookup(const bt_addr_le_t *addr) {
    for (size_t i = 0; i < largest_index_used; ++i) {
        if (!bt_addr_le_cmp(addr, &keys[i].addr)) {
            return &keys[i];
        }
    }
    return NULL;
}

static int space_settings_set(const char *key, size_t len_rd,
                              settings_read_cb read_cb, void *cb_arg) {
    const char *next;

    int key_len = settings_name_next(key, &next);
    if (!next) {
        bt_addr_le_t addr;
        if (!space_settings_decode_key(key, key_len, &addr)) {
            spacekey_t *slot = spacekey_lookup_add(&addr);
            if (!slot) {
                return -ENOSPC;
            }
            ssize_t len = read_cb(cb_arg, slot->key, BLAKE2S_KEYBYTES);
            if (!len) {
                memset(&slot->addr, 0, sizeof(slot->addr));
                return 0;
            }
            if (len != BLAKE2S_KEYBYTES) {
                return (len < 0) ? len : -EINVAL;
            }
            memcpy(&slot->addr, &addr, sizeof(bt_addr_le_t));
            LOG_INF("saved new spaceauth key");
            return 0;
        } else {
            return -EINVAL;
        }
    }
    return -ENOENT;
}

struct settings_handler space_settings_conf = {
        .name = "space",
        .h_set = space_settings_set
};

int spacekey_add(const bt_addr_le_t *addr, const uint8_t *key) {
    if (!bt_addr_cmp(&addr->a, &NO_ADDR)) {
        return -EINVAL;
    }
    char path[14];
    space_settings_encode_key(path, sizeof(path), addr);
    spacekey_t *slot = spacekey_lookup_add(addr);
    if (!slot) {
        return -ENOSPC;
    }
    memcpy(slot->key, key, BLAKE2S_KEYBYTES);
    settings_save_one(path, key, BLAKE2S_KEYBYTES);
    return 0;
}

int spacekey_del(const bt_addr_le_t *addr) {
    if (!bt_addr_cmp(&addr->a, &NO_ADDR)) {
        return -EINVAL;
    }
    spacekey_t *slot = spacekey_lookup(addr);
    if (!slot) {
        return -ENOENT;
    }
    char path[14];
    space_settings_encode_key(path, sizeof(path), addr);
    settings_delete(path);
    memset(&slot->addr, 0, sizeof(slot->addr));
    return 0;
}

int spaceauth_validate(const bt_addr_le_t *addr, const uint8_t *challenge, const uint8_t *response) {
    spacekey_t *slot = spacekey_lookup(addr);
    if (!slot) {
        return -ENOENT;
    }
    uint8_t correct_response[BLAKE2S_OUTBYTES];
    blake2s(correct_response, BLAKE2S_OUTBYTES, challenge, BLAKE2S_BLOCKBYTES, slot->key, BLAKE2S_KEYBYTES);
    if (!_compare(response, correct_response, BLAKE2S_BLOCKBYTES)) {
        return 0;
    } else {
        return -EINVAL;
    }
}

void spaceauth_init() {
    LOG_INF("initialize spaceauth");
    int err;

    err = settings_subsys_init();
    if (err) {
        LOG_ERR("settings_subsys_init failed (err %d)", err);
    }

    err = settings_register(&space_settings_conf);
    if (err) {
        LOG_ERR("ps_settings_register failed (err %d)", err);
    }
}