// zephyr includes
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <shell/shell.h>
#include <zephyr.h>
#include <hci_core.h> //use of internal hci API for 'bt_addr_le_is_bonded(id, addr)'
// own includes
#include "spaceauth.h"
#include "helper.h"

LOG_MODULE_REGISTER(app);

static struct bt_conn *default_conn = NULL;

// library of UUIDs
static struct bt_uuid_128 auth_service_uuid = BT_UUID_INIT_128(
        0xee, 0x8a, 0xcb, 0x07, 0x8d, 0xe1, 0xfc, 0x3b,
        0xfe, 0x8e, 0x69, 0x22, 0x41, 0xbe, 0x87, 0x66);
static struct bt_uuid_128 auth_challenge_uuid = BT_UUID_INIT_128(
        0xd5, 0x12, 0x7b, 0x77, 0xce, 0xba, 0xa7, 0xb1,
        0x86, 0x9a, 0x90, 0x47, 0x02, 0xc9, 0x3d, 0x95);
static struct bt_uuid_128 auth_response_uuid = BT_UUID_INIT_128(
        0x06, 0x3f, 0x0b, 0x51, 0xbf, 0x48, 0x4f, 0x95,
        0x92, 0xd7, 0x28, 0x5c, 0xd6, 0xfd, 0xd2, 0x2f);
static struct bt_uuid_16 gatt_ccc_uuid = BT_UUID_INIT_16(0x2902);

// save slots for discovered GATT handles
static uint16_t auth_svc_handle = 0;
static uint16_t auth_challenge_chr_handle = 0;
static uint16_t auth_challenge_chr_value_handle = 0;
static uint16_t auth_response_chr_handle = 0;
static uint16_t auth_response_chr_value_handle = 0;
static uint16_t auth_response_chr_ccc_handle = 0;

uint8_t challenge[64] = {0};
uint8_t response[32] = {0};

// pre-declaration of interesting functions in ideal order of events
static void bt_ready_cb(int err);

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
                         struct net_buf_simple *ad);

static void connected_cb(struct bt_conn *conn, u8_t err);

static void security_changed_cb(struct bt_conn *conn, bt_security_t level);

static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params);

static void write_func(struct bt_conn *conn, u8_t err,
                       struct bt_gatt_write_params *params);

static u8_t notify_func(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, u16_t length);

static u8_t read_func(struct bt_conn *conn, u8_t err,
                      struct bt_gatt_read_params *params,
                      const void *data, u16_t length);

static void disconnected_cb(struct bt_conn *conn, u8_t reason);

// params for read_func
static struct bt_gatt_read_params read_params;
// params for write_func
static struct bt_gatt_write_params write_params;
// params for discover_func
static struct bt_gatt_discover_params discover_params = {0};
// params for bt_gatt_subscribe
static struct bt_gatt_subscribe_params subscribe_params = {
        .value = BT_GATT_CCC_INDICATE,
        .flags = 0,
        .notify = notify_func,
};
// collection of conn callbacks
static struct bt_conn_cb conn_callbacks = {
        .connected = connected_cb,
        .disconnected = disconnected_cb,
        .security_changed = security_changed_cb,
};

static struct k_delayed_work timeout_timer;

// timeout function to kill connections that take too long
static void timeout(struct k_work *work) {
    LOG_ERR("TIMEOUT REACHED");
    if (default_conn) {
        bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

// helper function for advertisement data parser
bool ad_parse_func(struct bt_data *data, void *user_data) {
    int16_t *batt = user_data;
    if (data->type == BT_DATA_SVC_DATA16) {
        // check if this is service data for this battery service (uuid = 0x180f)
        if (data->data_len != 3 || data->data[0] != 0x0f || data->data[1] != 0x18) {
            return true;
        }
        *batt = data->data[2];
        return false; // stop parsing, found what we wanted
    }
    return true;
}

#define BT_LE_CONN_PARAM_LOW_TIMEOUT BT_LE_CONN_PARAM(BT_GAP_INIT_CONN_INT_MIN, \
                          BT_GAP_INIT_CONN_INT_MAX, \
                          0, 100)

/**
 * command to start up the BLE stack
 * @param shell shell issuing command
 * @return 0 on success, else error code of bt_enable
 */
static int cmd_ble_start(const struct shell *shell, size_t argc, char **argv) {
    int err = bt_enable(bt_ready_cb);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(NULL);
    LOG_INF("Bluetooth initialized");
    return 0;
}

SHELL_CMD_REGISTER(ble_start, NULL, "start ble", cmd_ble_start);

/**
 * gets called when the BLE stack finished initializing
 * starts scanning for devices
 * @param err possible error while initializing
 */
static void bt_ready_cb(int err) {
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
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

    LOG_DBG("Scanning successfully started");
}

/**
 * called when a device is found
 * connects to bonded devices
 * @param addr address of the device
 * @param rssi rssi of the device
 * @param type type of the advertisement
 * @param ad advertisement data
 */
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

    // read battery level from advertising data if available
    int8_t blvl = -1;
    bt_data_parse(ad, ad_parse_func, &blvl);
    if (blvl >= 0) {
        LOG_INF("Battery Level: %i%%", blvl);
    }

    LOG_DBG("Connecting to device...");

    int err = bt_le_scan_stop();
    if (err) {
        LOG_ERR("Couldn't stop scanning: %i", err);
        err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (err) {
            LOG_ERR("Scanning failed to start (err %d)", err);
        }
        return;
    }
    default_conn = bt_conn_create_le(addr, BT_LE_CONN_PARAM_LOW_TIMEOUT);
    if (!default_conn) {
        LOG_ERR("Couldn't connect: %i", err);
        err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (err) {
            LOG_ERR("Scanning failed to start (err %d)", err);
        }
    }
    LOG_DBG("Now, the connected callback should be called...");
}

// conn callbacks definition
/**
 * gets called when a connection is being established
 * starts security request and GATT discovery
 * on error or failed security request, continue scanning
 * @param conn new connection
 * @param err possible error code when establishing connection
 */
static void connected_cb(struct bt_conn *conn, u8_t err) {

    const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    if (err) {
        LOG_ERR("Failed to connect to [%02X:%02X:%02X:%02X:%02X:%02X] (%u)",
                addr->a.val[5], addr->a.val[4], addr->a.val[3], addr->a.val[2],
                addr->a.val[1], addr->a.val[0], err);
        if (conn == default_conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;
        }
        int error = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (error) {
            LOG_ERR("Scanning failed to start (err %d)", error);
        }
        return;
    }
    if (conn != default_conn) {
        LOG_ERR("New unhandled connection!");
        return;
    }

    // set up timeout
    k_delayed_work_init(&timeout_timer, timeout);
    k_delayed_work_submit(&timeout_timer, K_SECONDS(5));

    LOG_INF("Connected: [%02X:%02X:%02X:%02X:%02X:%02X]", addr->a.val[5],
            addr->a.val[4], addr->a.val[3], addr->a.val[2], addr->a.val[1],
            addr->a.val[0]);

    int ret = bt_conn_security(conn, BT_SECURITY_FIPS);
    if (ret) {
        LOG_ERR("Kill connection: insufficient security %i", ret);
        bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_SECURITY);
    } else {
        LOG_DBG("bt_conn_security successful");
    }

    LOG_DBG("Starting Discovery...");
    discover_params.uuid = &auth_service_uuid.uuid;
    discover_params.func = discover_func;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_ERR("Discover failed(err %d)", err);
        return;
    }
}

/**
 * gets called when the security level changes
 * only used for debugging
 * @param conn current connection
 * @param level new security level
 */
static void security_changed_cb(struct bt_conn *conn, bt_security_t level) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_DBG("Security changed: %s level %u", log_strdup(addr), level);
}


/**
 * gets called multiple times during GATT discovery when new handles are found etc.
 * @param conn current connection
 * @param attr found attribute
 * @param params given discovery params
 * @return BT_GATT_ITER_STOP
 */
static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params) {
    int err;

    if (attr) {
        LOG_DBG("[ATTRIBUTE] handle %u", attr->handle);

        if (!bt_uuid_cmp(params->uuid, &auth_service_uuid.uuid)) {
            LOG_DBG("found auth service handle %u", attr->handle);
            auth_svc_handle = attr->handle;
            //next up: search challenge chr
            discover_params.uuid = &auth_challenge_uuid.uuid;
            discover_params.start_handle = attr->handle + 1;
            discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("challenge chr discovery failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else if (!bt_uuid_cmp(params->uuid, &auth_challenge_uuid.uuid)) {
            LOG_DBG("found auth challenge chr handle %u", attr->handle);
            auth_challenge_chr_handle = attr->handle;
            auth_challenge_chr_value_handle = attr->handle + 1;
            //next up: search response chr
            discover_params.start_handle = attr->handle + 2;
            discover_params.uuid = &auth_response_uuid.uuid;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("response chr discovery failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else if (!bt_uuid_cmp(params->uuid, &auth_response_uuid.uuid)) {
            LOG_DBG("found auth response chr handle %u", attr->handle);
            auth_response_chr_handle = attr->handle;
            auth_response_chr_value_handle = attr->handle + 1;
            subscribe_params.value_handle = attr->handle + 1;

            //next up: search response chr cccd
            discover_params.start_handle = attr->handle + 2;
            discover_params.uuid = &gatt_ccc_uuid.uuid;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("response chr cccd discovery failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else if (!bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC)) {
            LOG_DBG("found auth response chr cccd handle %u", attr->handle);
            auth_response_chr_ccc_handle = attr->handle;
            subscribe_params.ccc_handle = attr->handle;

            err = bt_gatt_subscribe(default_conn, &subscribe_params);
            if (err && err != -EALREADY) {
                LOG_ERR("Subscribe failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            } else {
                LOG_DBG("[SUBSCRIBED]");
            }

            LOG_INF("Discover complete");
            (void) memset(params, 0, sizeof(*params));

            if (bt_conn_enc_key_size(conn) == 0) {
                LOG_ERR("NO ENCRYPTION ENABLED. REFUSE TO SEND CHALLENGE.");
                bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_SECURITY);
            }

            LOG_DBG("Generating new challenge");
            bt_rand(challenge, 64);
            LOG_DBG("Writing challenge");
            write_params.func = write_func;
            write_params.handle = auth_challenge_chr_value_handle;
            write_params.length = 64;
            write_params.data = challenge;
            write_params.offset = 0;
            err = bt_gatt_write(conn, &write_params);
            if (err) {
                LOG_ERR("Challenge write failed(err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }

        }
    }
    return BT_GATT_ITER_STOP;
}

/**
 * gets called when the GATT clients finishes writing the challenge
 * resets write params
 * @param conn current connection
 * @param err error while writing
 * @param params given write params
 */
static void write_func(struct bt_conn *conn, u8_t err,
                       struct bt_gatt_write_params *params) {
    LOG_DBG("Write complete: err %u", err);
    (void) memset(&write_params, 0, sizeof(write_params));
}

/**
 * gets called when the peripheral notifies that the response is ready
 * starts reading the response
 * @param conn current connection
 * @param params given subscribe params
 * @param data data included
 * @param length
 * @return
 */
static u8_t notify_func(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, u16_t length) {
    if (!data) {
        LOG_DBG("[UNSUBSCRIBED]");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    LOG_DBG("[NOTIFICATION] data %p length %u", data, length);
    LOG_HEXDUMP_DBG(data, length, "Received data");
    if (length <= sizeof(response)) {
        LOG_INF("Coin notified that response is ready.");
        memcpy(response, data, length);
        if (length < sizeof(response)) {
            read_params.func = read_func;
            read_params.handle_count = 1;
            read_params.single.offset = 0;
            read_params.single.handle = auth_response_chr_value_handle;
            int err = bt_gatt_read(default_conn, &read_params);
            if (err) {
                LOG_ERR("Could not read response: %i", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        }
    }
    return BT_GATT_ITER_CONTINUE;
}

/**
 * gets called when a block of the response has been read
 * when response is read completely, checks response and kills connection
 * @param conn current connection
 * @param err error when reading
 * @param params given read params
 * @param data read data bytes
 * @param length length of data
 * @return BT_GATT_ITER_STOP when finished reading, else BT_GATT_ITER_CONTINUE
 */
static u8_t read_func(struct bt_conn *conn, u8_t err,
                      struct bt_gatt_read_params *params,
                      const void *data, u16_t length) {
    if (data) {
        LOG_DBG("Read complete: err %u length %u offset %u", err, length, params->single.offset);
        LOG_HEXDUMP_DBG(data, length, "Received data");
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

/**
 * gets called when an existing connection ended
 * does cleanup and continue scanning
 * @param conn old connection
 * @param reason reason to kill connection
 */
static void disconnected_cb(struct bt_conn *conn, u8_t reason) {
    const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    int err;
    if (conn != default_conn) {
        LOG_ERR("Disconnected from unknown connection");
        return;
    }
    LOG_INF("Disconnected: [%02X:%02X:%02X:%02X:%02X:%02X] (reason %u)",
            addr->a.val[5], addr->a.val[4], addr->a.val[3], addr->a.val[2],
            addr->a.val[1], addr->a.val[0], reason);

    k_delayed_work_cancel(&timeout_timer);

    if (default_conn) {
        bt_conn_unref(default_conn);
    }

    default_conn = NULL;

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
    }
}

void main(void) {
    spaceauth_init();
}
