#include <zephyr.h>
#include <shell/shell.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(app);

#include <bluetooth/bluetooth.h>
#include <settings/settings.h>
#include <power/reboot.h>

#include <keys.h> //use of internal keys API
#include <settings.h> //use of internal settings API
#include <hci_core.h> //use of internal hci API

#include <storage/flash_map.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>

#include "spaceauth.h"
#include "helper.h"

static struct bt_conn *default_conn;

static int cmd_coin_add(const struct shell *shell, size_t argc, char **argv) {
    shell_print(shell, "argc = %d", argc);
    for (size_t cnt = 0; cnt < argc; cnt++) {
        shell_print(shell, "  argv[%d] = %s", cnt, argv[cnt]);
    }
    struct bt_keys keys = {
            .keys = (BT_KEYS_IRK | BT_KEYS_LTK_P256),
            .flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC),
            .id = BT_ID_DEFAULT,
            .enc_size = BT_ENC_KEY_SIZE_MAX,
    };
    uint8_t spacekey[32] = {0};

    if (argc != 5) {
        LOG_ERR("incorrect number of arguments");
        return EINVAL;
    }

    // address
    int ret = parse_addr(argv[1], &keys.addr);
    if (ret) {
        return ret;
    }
    LOG_INF("valid address");
    // identity resolving key
    ret = parse_hex(argv[2], 16, keys.irk.val);
    if (ret) {
        return ret;
    }
    LOG_INF("valid IRK");
    // long-term key
    ret = parse_hex(argv[3], 16, keys.ltk.val);
    if (ret) {
        return ret;
    }
    LOG_INF("valid LTK");
    // space key
    ret = parse_hex(argv[4], 32, spacekey);
    if (ret) {
        return ret;
    }
    LOG_INF("valid space key");

    ret = spacekey_add(&keys.addr, spacekey);
    if (ret) {
        LOG_ERR("spacekey_add failed with %i", ret);
        return ret;
    }
    char key[BT_SETTINGS_KEY_MAX];
    bt_settings_encode_key(key, sizeof(key), "keys", &keys.addr, NULL);
    bt_keys_store(&keys);
    settings_load_subtree(key);
    return 0;
}

static int cmd_coin_del(const struct shell *shell, size_t argc, char **argv) {
    bt_addr_le_t addr;
    if (argc != 2) {
        LOG_ERR("incorrect number of arguments");
        return EINVAL;
    }
    int ret = parse_addr(argv[1], &addr);
    if (ret) {
        return ret;
    }
    LOG_INF("valid address");
    char addr_str[BT_ADDR_LE_STR_LEN] = "foo";
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    shell_print(shell, "parsed BLE addr [%s]", addr_str);

    ret = bt_unpair(BT_ID_DEFAULT, &addr);
    if (ret) {
        LOG_ERR("could not unpair this address (err %d)", ret);
    }
    spacekey_del(&addr);

    return 0;
}
/* Creating subcommands (level 1 command) array for command "coin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_coin,
                               SHELL_CMD(add, NULL, "usage: coin add <addr> <irk> <ltk> <spacekey>",
                                         cmd_coin_add),
                               SHELL_CMD(del, NULL, "usage: coin del <addr> ", cmd_coin_del),
                               SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "coin" */
SHELL_CMD_REGISTER(coin, &sub_coin, "commands to manage coins", NULL);

static int cmd_print_spacekeys(const struct shell *shell, size_t argc, char **argv) {
    spacekeys_print(shell);
}

static void print_bond_func(struct bt_keys *keys, void *data) {
    const struct shell *shell = data;
    shell_print(shell, "[%02X:%02X:%02X:%02X:%02X:%02X] keys: %u, flags: %u", keys->addr.a.val[5], keys->addr.a.val[4],
                keys->addr.a.val[3], keys->addr.a.val[2], keys->addr.a.val[1], keys->addr.a.val[0], keys->keys,
                keys->flags);
}

static int cmd_print_bonds(const struct shell *shell, size_t argc, char **argv) {
    shell_print(shell, "printing all bonds...");
    bt_keys_foreach(BT_KEYS_ALL, print_bond_func, shell);
}

static int cmd_settings_load(const struct shell *shell, size_t argc, char **argv) {
    settings_load();
}

static int cmd_settings_clear(const struct shell *shell, size_t argc, char **argv) {
    const struct flash_area *fap;
    flash_area_open(DT_FLASH_AREA_STORAGE_ID, &fap);
    int rc = flash_area_erase(fap, 0, fap->fa_size);
    if (rc != 0) {
        LOG_ERR("cannot get flash area");
    }
    shell_error(shell, "Storage cleared, please reboot. No, loading does not suffice.");
}

static int cmd_reboot(const struct shell *shell, size_t argc, char **argv) {
    sys_reboot(SYS_REBOOT_COLD);
}


/* Creating subcommands (level 1 command) array for command "coin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_stats,
                               SHELL_CMD(spacekey, NULL, "prints space keys", cmd_print_spacekeys),
                               SHELL_CMD(bonds, NULL, "prints bonds", cmd_print_bonds),
                               SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "coin" */
SHELL_CMD_REGISTER(stats, &sub_stats, "commands to print internal state", NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_settings,
                               SHELL_CMD(load, NULL, "load all settings from storage", cmd_settings_load),
                               SHELL_CMD(clear, NULL, "clear storage (needs reboot)", cmd_settings_clear),
                               SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(settings, &sub_settings, "commands to manage settings", NULL);

SHELL_CMD_REGISTER(reboot, NULL, "perform cold system reboot", cmd_reboot);


static int cmd_central_setup(const struct shell *shell, size_t argc, char **argv) {
    bt_addr_le_t addr;
    int ret = parse_addr(argv[1], &addr);
    if (ret) {
        return ret;
    }
    LOG_INF("valid address");
    uint8_t irk[16];
    ret = parse_hex(argv[2], 16, irk);
    if (ret) {
        return ret;
    }
    LOG_INF("valid IRK");
    settings_save_one("bt/irk", irk, sizeof(irk));
    settings_save_one("bt/id", &addr, sizeof(bt_addr_le_t));
    settings_load_subtree("bt/irk");
    settings_load_subtree("bt/id");
}

SHELL_CMD_REGISTER(central_setup, NULL, "usage: central_setup <addr> <irk>", cmd_central_setup);

static struct bt_uuid_128 auth_service_uuid = BT_UUID_INIT_128(
        0xee, 0x8a, 0xcb, 0x07, 0x8d, 0xe1, 0xfc, 0x3b,
        0xfe, 0x8e, 0x69, 0x22, 0x41, 0xbe, 0x87, 0x66);

static struct bt_uuid_128 auth_challenge_uuid = BT_UUID_INIT_128(
        0xd5, 0x12, 0x7b, 0x77, 0xce, 0xba, 0xa7, 0xb1,
        0x86, 0x9a, 0x90, 0x47, 0x02, 0xc9, 0x3d, 0x95);

static struct bt_uuid_128 auth_response_uuid = BT_UUID_INIT_128(
        0x06, 0x3f, 0x0b, 0x51, 0xbf, 0x48, 0x4f, 0x95,
        0x92, 0xd7, 0x28, 0x5c, 0xd6, 0xfd, 0xd2, 0x2f);

static struct bt_uuid_16 bas_service_uuid = BT_UUID_INIT_16(0x180f);
static struct bt_uuid_16 bas_blvl_uuid = BT_UUID_INIT_16(0x2a19);
static struct bt_uuid_16 gatt_ccc_uuid = BT_UUID_INIT_16(0x2902);

static uint16_t bas_svc_handle = 0;
static uint16_t bas_blvl_chr_handle = 0;
static uint16_t bas_blvl_chr_val_handle = 0;
static uint16_t auth_svc_handle = 0;
static uint16_t auth_challenge_chr_handle = 0;
static uint16_t auth_challenge_chr_value_handle = 0;
static uint16_t auth_response_chr_handle = 0;
static uint16_t auth_response_chr_value_handle = 0;
static uint16_t auth_response_chr_ccc_handle = 0;

uint8_t challenge[64] = {0};

uint8_t response[32] = {0};

static struct bt_gatt_read_params read_params;

static u8_t read_func(struct bt_conn *conn, u8_t err,
                      struct bt_gatt_read_params *params,
                      const void *data, u16_t length) {
    if (data) {
        LOG_INF("Read complete: err %u length %u offset %u", err, length, params->single.offset);
        LOG_HEXDUMP_INF(data, length, "Received data");
        if (params->single.handle == auth_response_chr_value_handle) {
            if (params->single.offset + length <= sizeof(response)) {
                memcpy(response + params->single.offset, data, length);
                if (params->single.offset + length == sizeof(response)) {
                    if (spaceauth_validate(bt_conn_get_dst(conn), challenge, response) == 0) {
                        LOG_INF("KEY AUTHENTICATED. OPEN DOOR PLEASE.");
                    }
                    memset(challenge, 0, sizeof(challenge));
                    memset(response, 0, sizeof(response));
                    bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                    return BT_GATT_ITER_STOP;
                }
            }
        }
    }
    return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_write_params write_params;

static void write_func(struct bt_conn *conn, u8_t err,
                       struct bt_gatt_write_params *params) {
    LOG_INF("Write complete: err %u", err);
    (void) memset(&write_params, 0, sizeof(write_params));
}

static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params);

static u8_t notify_func(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, u16_t length) {
    if (!data) {
        LOG_INF("[UNSUBSCRIBED]");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    LOG_INF("[NOTIFICATION] data %p length %u", data, length);
    LOG_HEXDUMP_INF(data, length, "Received data");
    if (length <= sizeof(response)) {
        memcpy(response, data, length);
        if (length < sizeof(response)) {
            read_params.func = read_func;
            read_params.handle_count = 1;
            read_params.single.offset = 0;
            read_params.single.handle = auth_response_chr_value_handle;
            int err = bt_gatt_read(default_conn, &read_params);
            if (err) {
                LOG_ERR("Could not read response");
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        }
    }
    return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_discover_params discover_params = {0};
static struct bt_gatt_subscribe_params subscribe_params = {
        .value = BT_GATT_CCC_INDICATE,
        .flags = 0,
        .notify = notify_func,
};

static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params) {
    int err;

    if (attr) {
        LOG_INF("[ATTRIBUTE] handle %u", attr->handle);

        if (!bt_uuid_cmp(params->uuid, &auth_service_uuid.uuid)) {
            LOG_INF("found auth service handle");
            auth_svc_handle = attr->handle;
            //next up: search challenge chr
            discover_params.uuid = &auth_challenge_uuid.uuid;
            discover_params.start_handle = attr->handle + 1;
            discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("challenge chr discovery failed (err %d)", err);
            }
        } else if (!bt_uuid_cmp(params->uuid, &auth_challenge_uuid.uuid)) {
            LOG_INF("found auth challenge chr handle");
            auth_challenge_chr_handle = attr->handle;
            auth_challenge_chr_value_handle = attr->handle + 1;
            //next up: search response chr
            discover_params.start_handle = attr->handle + 2;
            discover_params.uuid = &auth_response_uuid.uuid;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("challenge chr discovery failed (err %d)", err);
            }
        } else if (!bt_uuid_cmp(params->uuid, &auth_response_uuid.uuid)) {
            LOG_INF("found auth response chr handle");
            auth_response_chr_handle = attr->handle;
            auth_response_chr_value_handle = attr->handle + 1;
            subscribe_params.value_handle = attr->handle + 1;

            //next up: search response chr cccd
            discover_params.start_handle = attr->handle + 2;
            discover_params.uuid = &gatt_ccc_uuid.uuid;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_INF("Discover failed (err %d)", err);
            }
        } else if (!bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC)) {
            LOG_INF("found auth response chr cccd handle");
            auth_response_chr_ccc_handle = attr->handle;
            subscribe_params.ccc_handle = attr->handle;

            err = bt_gatt_subscribe(default_conn, &subscribe_params);
            if (err && err != -EALREADY) {
                LOG_INF("Subscribe failed (err %d)", err);
            } else {
                LOG_INF("[SUBSCRIBED]");
            }

            //next up: search bas svc
            discover_params.uuid = &bas_service_uuid.uuid;
            discover_params.start_handle = 0x0001;
            discover_params.type = BT_GATT_DISCOVER_PRIMARY;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_INF("Discover failed (err %d)", err);
            }
        } else if (!bt_uuid_cmp(params->uuid, BT_UUID_BAS)) {
            LOG_INF("found bas svc handle");
            bas_svc_handle = attr->handle;

            //next up: search bas blvl chr
            discover_params.start_handle = attr->handle + 1;
            discover_params.uuid = &bas_blvl_uuid.uuid;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_INF("Discover failed (err %d)", err);
            }
        } else if (!bt_uuid_cmp(params->uuid, BT_UUID_BAS_BATTERY_LEVEL)) {
            LOG_INF("found bas blvl chr handle");
            bas_blvl_chr_handle = attr->handle;
            bas_blvl_chr_val_handle = attr->handle + 1;
            LOG_INF("Discover complete");
            (void) memset(params, 0, sizeof(*params));

            bt_rand(challenge, 64);

            write_params.func = write_func;
            write_params.handle = auth_challenge_chr_value_handle;
            write_params.length = 64;
            write_params.data = challenge;
            write_params.offset = 0;
            err = bt_gatt_write(conn, &write_params);
            if (err) {
                LOG_INF("Challenge write failed(err %d)", err);
            }

        }
    }
    return BT_GATT_ITER_STOP;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
                         struct net_buf_simple *ad) {

    if (default_conn) {
        LOG_ERR("Already have a pending connection!");
        return;
    }
    LOG_INF("Device found: [%02X:%02X:%02X:%02X:%02X:%02X] (RSSI %d) (TYPE %u) "
            "(BONDED %u)",
            addr->a.val[5], addr->a.val[4], addr->a.val[3], addr->a.val[2],
            addr->a.val[1], addr->a.val[0], rssi, type,
            bt_addr_le_is_bonded(BT_ID_DEFAULT, addr));

    /* We're only interested in directed connectable events from bonded devices*/
    if ((type != BT_LE_ADV_DIRECT_IND && type != BT_LE_ADV_IND) ||
        !bt_addr_le_is_bonded(BT_ID_DEFAULT, addr)) {
        return;
    }

    LOG_INF("Connecting to device...");

    int err = bt_le_scan_stop();
    if (err) {
        LOG_ERR("Couldn't stop scanning: %i", err);
        err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (err) {
            LOG_ERR("Scanning failed to start (err %d)", err);
        }
        return;
    }
    default_conn = bt_conn_create_le(addr, BT_LE_CONN_PARAM_DEFAULT);
    if (!default_conn) {
        LOG_ERR("Couldn't connect: %i", err);
        err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (err) {
            LOG_ERR("Scanning failed to start (err %d)", err);
        }
    }
    LOG_INF("Now, the connected callback should be called...");
}

static void connected(struct bt_conn *conn, u8_t err) {

    const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    if (err) {
        LOG_INF("Failed to connect to [%02X:%02X:%02X:%02X:%02X:%02X] (%u)",
                addr->a.val[5], addr->a.val[4], addr->a.val[3], addr->a.val[2],
                addr->a.val[1], addr->a.val[0], err);
        if (conn == default_conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;
        }
        int error = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (error) {
            LOG_INF("Scanning failed to start (err %d)", error);
        }
        return;
    }
    if (conn != default_conn) {
        LOG_ERR("New unhandled connection!");
        return;
    }


    LOG_INF("Connected: [%02X:%02X:%02X:%02X:%02X:%02X]", addr->a.val[5],
            addr->a.val[4], addr->a.val[3], addr->a.val[2], addr->a.val[1],
            addr->a.val[0]);

    int ret = bt_conn_security(conn, BT_SECURITY_FIPS);
    if (ret) {
        LOG_INF("Kill connection: insufficient security %i", ret);
        bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_SECURITY);
    } else {
        LOG_INF("bt_conn_security successful");
    }

    LOG_INF("Starting Discovery...");
    discover_params.uuid = &auth_service_uuid.uuid;
    discover_params.func = discover_func;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_INF("Discover failed(err %d)", err);
        return;
    }
}

static void disconnected(struct bt_conn *conn, u8_t reason) {
    const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    int err;
    if (conn != default_conn) {
        LOG_ERR("Disconnected from unknown connection");
        return;
    }
    LOG_INF("Disconnected: [%02X:%02X:%02X:%02X:%02X:%02X] (reason %u)",
            addr->a.val[5], addr->a.val[4], addr->a.val[3], addr->a.val[2],
            addr->a.val[1], addr->a.val[0], reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
    }

    default_conn = NULL;

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Security changed: %s level %u", log_strdup(addr), level);
}

static struct bt_conn_cb conn_callbacks = {
        .connected = connected,
        .disconnected = disconnected,
        .security_changed = security_changed,
};

static void bt_ready(int err) {
    if (err) {
        LOG_INF("Bluetooth init failed (err %d)", err);
        return;
    }

    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);

    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return;
    }

    LOG_INF("Scanning successfully started");
}

static int cmd_ble_start(const struct shell *shell, size_t argc, char **argv) {
    int err = bt_enable(bt_ready);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(NULL);
    LOG_INF("Bluetooth initialized");
}

SHELL_CMD_REGISTER(ble_start, NULL, "start ble", cmd_ble_start);


void main(void) {
    spaceauth_init();
}
