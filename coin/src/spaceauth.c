#include <zephyr.h>
#include <settings/settings.h>
#include <bluetooth/gatt.h>

#include "blake2.h"
#include "spaceauth.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(spaceauth);


static struct bt_uuid_128 auth_service_uuid = BT_UUID_INIT_128(
        0xee, 0x8a, 0xcb, 0x07, 0x8d, 0xe1, 0xfc, 0x3b,
        0xfe, 0x8e, 0x69, 0x22, 0x41, 0xbe, 0x87, 0x66);

static struct bt_uuid_128 auth_challenge_uuid = BT_UUID_INIT_128(
        0xd5, 0x12, 0x7b, 0x77, 0xce, 0xba, 0xa7, 0xb1,
        0x86, 0x9a, 0x90, 0x47, 0x02, 0xc9, 0x3d, 0x95);

static struct bt_uuid_128 auth_response_uuid = BT_UUID_INIT_128(
        0x06, 0x3f, 0x0b, 0x51, 0xbf, 0x48, 0x4f, 0x95,
        0x92, 0xd7, 0x28, 0x5c, 0xd6, 0xfd, 0xd2, 0x2f);

static uint8_t auth_key[BLAKE2S_KEYBYTES] = {0};
static uint8_t challenge[BLAKE2S_BLOCKBYTES] = {0};
static uint8_t response[BLAKE2S_OUTBYTES] = {0};

static u16_t ccc_value;

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value) {
    ccc_value = value;
}

static void indicate_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        u8_t err) {
    if (err != 0U) {
        LOG_ERR("indication fail: %i", err);
    } else {
        LOG_INF("indication success");
    }
}

static struct bt_gatt_indicate_params ind_params = {.data=response, .len=BLAKE2S_OUTBYTES, .attr=NULL, .func=&indicate_cb};

static ssize_t write_challenge(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               const void *buf, u16_t len,
                               u16_t offset, u8_t flags);

static ssize_t read_response(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             void *buf, u16_t len,
                             u16_t offset) {
    const char *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                             BLAKE2S_OUTBYTES);
}

BT_GATT_SERVICE_DEFINE(auth_svc,
                       BT_GATT_PRIMARY_SERVICE(&auth_service_uuid),
                       BT_GATT_CHARACTERISTIC(&auth_challenge_uuid.uuid, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_AUTH,
                                              BT_GATT_PERM_WRITE_AUTHEN | BT_GATT_PERM_WRITE_ENCRYPT,
                                              NULL, write_challenge, challenge),
                       BT_GATT_CHARACTERISTIC(&auth_response_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_INDICATE,
                                              BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_READ_ENCRYPT,
                                              read_response, NULL, response),
                       BT_GATT_CCC(ccc_cfg_changed)
);

static ssize_t write_challenge(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               const void *buf, u16_t len,
                               u16_t offset, u8_t flags) {
    if (offset + len > BLAKE2S_BLOCKBYTES) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(challenge + offset, buf, len);
    LOG_INF("write challenge offset: %i, len: %i", offset, len);

    if (offset + len == BLAKE2S_BLOCKBYTES) {
        blake2s(response, BLAKE2S_OUTBYTES, challenge, BLAKE2S_BLOCKBYTES, auth_key, BLAKE2S_KEYBYTES);
        if (ccc_value == BT_GATT_CCC_INDICATE) {
            ind_params.attr = &auth_svc.attrs[4];
            u16_t mtu = bt_gatt_get_mtu(conn);
            ind_params.len = MIN(mtu - 3, BLAKE2S_OUTBYTES);
            LOG_INF("connection has MTU: %u", mtu);
            bt_gatt_indicate(NULL, &ind_params);
        }
    }

    return len;
}

//settings stuff
static int set(const char *key, size_t len_rd,
               settings_read_cb read_cb, void *cb_arg) {
    ARG_UNUSED(len_rd);
    const char *next;

    size_t key_len = (size_t) settings_name_next(key, &next);
    if (!next) {
        if (!strncmp(key, "key", key_len)) {
            ssize_t len = read_cb(cb_arg, auth_key, BLAKE2S_KEYBYTES);
            if (len != BLAKE2S_KEYBYTES) {
                memset(auth_key, 0, BLAKE2S_KEYBYTES);
                return (len < 0) ? len : -EINVAL;
            }
            LOG_INF("loaded spacekey");
            return 0;
        }
    }
    return -ENOENT;
}

static struct settings_handler auth_settings = {
        .name = "space",
        .h_set = set,
};

void space_auth_init(void) {
    LOG_INF("initialize space auth");
    int err;

    err = settings_subsys_init();
    if (err) {
        LOG_ERR("settings_subsys_init failed (err %d)", err);
    }

    err = settings_register(&auth_settings);
    if (err) {
        LOG_ERR("ps_settings_register failed (err %d)", err);
    }
}
