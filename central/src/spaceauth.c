#include "spaceauth.h"
#include "tinycrypt/utils.h"
#include "blake2.h"
#include "bluetooth/gatt.h"

#include "guestlist.h"
#include "hci_core.h"

struct space_data {
    bt_addr_le_t id;
    uint8_t key[BLAKE2S_KEYBYTES];
};

static struct space_data periphs[BT_GATT_CCC_MAX];
static size_t periph_count = 0;

bool id_exists(const bt_addr_le_t *addr) {

    for (size_t i = 0; i < periph_count; ++i) {
        if (!bt_addr_le_cmp(&periphs[i].id, addr)) {
            return true;
        }
        return false;
    }
}

bool space_add_id(struct bt_keys *keys, uint8_t *key) {
    if (!id_exists(&keys->addr)) {
        memcpy(periphs[periph_count].key, key, BLAKE2S_KEYBYTES);
        bt_addr_le_copy(&periphs[periph_count].id, &keys->addr);
        struct bt_keys* dest = bt_keys_get_addr(BT_ID_DEFAULT,&keys->addr);
        if(!dest){
            fprintf(stderr,"Unable to save key!\n");
            return false;
        }
        memcpy(dest,keys, sizeof(struct bt_keys)); //copy key into host keystore
        bt_id_add(dest); //tell controller about key
        periph_count++;
        return true;
    }
    return false;
}

bool check_sign(const bt_addr_le_t *addr, const uint8_t *challenge, const uint8_t *remote_sign) {
    size_t i = 0;
    while (i < BT_GATT_CCC_MAX) {
        if (!bt_addr_le_cmp(&periphs[i].id, addr)) {
            uint8_t buf[BLAKE2S_OUTBYTES];
            blake2s(buf, BLAKE2S_OUTBYTES, challenge, BLAKE2S_BLOCKBYTES, periphs[i].key, BLAKE2S_KEYBYTES);
            if (_compare(buf, remote_sign, BLAKE2S_OUTBYTES) == 0) {
                return true;
            } else {
                return false;
            }
        }
        ++i;
    }
    return false;
}
