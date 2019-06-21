#include "spaceauth.h"
#include "tinycrypt/utils.h"
#include "blake2.h"
#include "bluetooth/gatt.h"

#include "hci_core.h"

#ifndef CONFIG_BT_MAX_PAIRED
#define CONFIG_BT_MAX_PAIRED -1
#endif

struct space_data {
    bt_addr_le_t id;
    uint8_t key[BLAKE2S_KEYBYTES];
};

static struct space_data periphs[CONFIG_BT_MAX_PAIRED];
static size_t periph_count = 0;

struct space_data *id_exists(const bt_addr_le_t *addr) {

    for (size_t i = 0; i < periph_count; ++i) {
        if (!bt_addr_le_cmp(&periphs[i].id, addr)) {
            return periphs + i;
        }
    }
    return NULL;
}


bool space_add_id(struct bt_keys *keys, uint8_t *key) {
    if (!id_exists(&keys->addr)) {
        memcpy(periphs[periph_count].key, key, BLAKE2S_KEYBYTES);
        bt_addr_le_copy(&periphs[periph_count].id, &keys->addr);
        struct bt_keys *dest = bt_keys_get_addr(BT_ID_DEFAULT, &keys->addr);
        if (!dest) {
            fprintf(stderr, "Unable to save key!\n");
            return false;
        }
        memcpy(dest, keys, sizeof(struct bt_keys)); //copy key into host keystore
        bt_id_add(dest); //tell controller about key
        periph_count++;
        return true;
    }
    return false;
}

void space_save_id(struct bt_keys *keys, void *arg) {
    FILE *fp = arg;
    struct space_data *s = id_exists(&keys->addr);
    if (s) {
        serialize_keys(keys, s->key, fp);
    }
}

bool check_sign(const bt_addr_le_t *addr, const uint8_t *challenge, const uint8_t *remote_sign) {
    struct space_data *s = id_exists(addr);
    if (s) {
        uint8_t buf[BLAKE2S_OUTBYTES];
        blake2s(buf, BLAKE2S_OUTBYTES, challenge, BLAKE2S_BLOCKBYTES, s->key, BLAKE2S_KEYBYTES);
        if (_compare(buf, remote_sign, BLAKE2S_OUTBYTES) == 0) {
            return true;
        }
    }
    return false;
}
