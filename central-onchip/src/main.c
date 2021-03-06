// zephyr includes
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <shell/shell.h>
#include <drivers/watchdog.h>
#include <zephyr.h>
#include <hci_core.h> //use of internal hci API for 'bt_addr_le_is_bonded(id, addr)'
// own includes
#include "spaceauth.h"
#include "helper.h"
#include "leds.h"

LOG_MODULE_REGISTER(app);

static struct bt_conn *default_conn = NULL;

// library of UUIDs
#define UUID_AUTH_SERVICE      BT_UUID_DECLARE_128(0xee, 0x8a, 0xcb, 0x07, 0x8d, 0xe1, 0xfc, 0x3b, \
                                                   0xfe, 0x8e, 0x69, 0x22, 0x41, 0xbe, 0x87, 0x66)
#define UUID_AUTH_CHALLENGE    BT_UUID_DECLARE_128(0xd5, 0x12, 0x7b, 0x77, 0xce, 0xba, 0xa7, 0xb1, \
                                                   0x86, 0x9a, 0x90, 0x47, 0x02, 0xc9, 0x3d, 0x95)
#define UUID_AUTH_RESPONSE     BT_UUID_DECLARE_128(0x06, 0x3f, 0x0b, 0x51, 0xbf, 0x48, 0x4f, 0x95, \
                                                   0x92, 0xd7, 0x28, 0x5c, 0xd6, 0xfd, 0xd2, 0x2f)

static struct bt_uuid_16 uuid_16 = {{0}};
static struct bt_uuid_128 uuid_128 = {{0}};


// save slots for discovered GATT handles
static uint16_t auth_response_chr_value_handle = 0;
static uint16_t auth_challenge_chr_value_handle = 0;

uint8_t challenge[64] = {0};
uint8_t response[32] = {0};

// pre-declaration of interesting functions in ideal order of events
static void bt_ready_cb(int err);

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
                         struct net_buf_simple *ad);

static void connected_cb(struct bt_conn *conn, u8_t err);

static void security_changed_cb(struct bt_conn *conn, bt_security_t level,
                                enum bt_security_err err);

static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params);

static void write_completed_func(struct bt_conn *conn, u8_t err,
                                 struct bt_gatt_write_params *params);

static u8_t notify_func(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, u16_t length);

static u8_t read_completed_func(struct bt_conn *conn, u8_t err,
                                struct bt_gatt_read_params *params,
                                const void *data, u16_t length);

static void disconnected_cb(struct bt_conn *conn, u8_t reason);

// switch to prevent the discovery to be started twice
static bool security_established = false;
// params for read_completed_func
static struct bt_gatt_read_params read_params = {{{0}}};
// params for write_completed_func
static struct bt_gatt_write_params write_params = {{{0}}};
// params for discover_func
static struct bt_gatt_discover_params discover_params = {{{0}}};
// params for bt_gatt_subscribe
static struct bt_gatt_subscribe_params subscribe_params = {{{0}}};
// collection of conn callbacks
static struct bt_conn_cb conn_callbacks = {
        .connected = connected_cb,
        .disconnected = disconnected_cb,
        .security_changed = security_changed_cb,
};

static struct k_delayed_work timeout_timer = {{0}};

// timeout function to kill connections that take too long
static void timeout(struct k_work *work) {
    ARG_UNUSED(work);
    LOG_ERR("TIMEOUT REACHED");
    if (default_conn) {
        bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

struct device *wdt;
int wdt_channel_id;
struct wdt_timeout_cfg wdt_config = {
        .flags = WDT_FLAG_RESET_SOC,
        .window.min = 0U,
        .window.max = 5000U
};

static bool i_want_to_die = false;

static struct k_timer watchdog_timer;

static void watchdog_timer_expiry_function(struct k_timer *timer_id) {
    ARG_UNUSED(timer_id);
    if (!i_want_to_die) {
        wdt_feed(wdt, wdt_channel_id);
    }
}

// helper function for advertisement data parser
const static size_t BT_ADV_BLVL_IDX = 2;

bool ad_parse_func(struct bt_data *data, void *user_data) {
    int16_t *batt = user_data;
    if (data->type == BT_DATA_SVC_DATA16) {
        // check if this is service data for this battery service (uuid = 0x180f, one byte of data)
        if (data->data_len != 3 || data->data[0] != 0x0f || data->data[1] != 0x18) {
            return true;
        }
        *batt = data->data[BT_ADV_BLVL_IDX];
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
    ARG_UNUSED(shell);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    helper_ble_running();
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
            addr->a.val[5], addr->a.val[4], addr->a.val[3],
            addr->a.val[2], addr->a.val[1], addr->a.val[0],
            rssi, type, bt_addr_le_is_bonded(BT_ID_DEFAULT, addr));

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
    security_established = false;
    const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    if (err) {
        LOG_ERR("Failed to connect to [%02X:%02X:%02X:%02X:%02X:%02X] (%u)",
                addr->a.val[5], addr->a.val[4], addr->a.val[3],
                addr->a.val[2], addr->a.val[1], addr->a.val[0],
                err);

        if (default_conn) {
            bt_conn_unref(default_conn);
        }
        default_conn = NULL;

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
    led0_set(0);
    led1_set(1, 0, 0);

    // set up timeout
    k_delayed_work_submit(&timeout_timer, K_SECONDS(5));

    LOG_INF("Connected: [%02X:%02X:%02X:%02X:%02X:%02X]",
            addr->a.val[5], addr->a.val[4], addr->a.val[3],
            addr->a.val[2], addr->a.val[1], addr->a.val[0]);

    int ret = bt_conn_set_security(conn, BT_SECURITY_L4);
    if (ret) {
        LOG_ERR("Kill connection: insufficient security %i", ret);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    } else {
        LOG_INF("bt_conn_security successful");
    }
}

/**
 * gets called when the security level changes
 * @param conn current connection
 * @param level new security level
 */
static void security_changed_cb(struct bt_conn *conn, bt_security_t level,
                                enum bt_security_err err) {
    if (!security_established && conn == default_conn) {
        LOG_DBG("Security changed: level %u", level);
        if (err == 0 && level == 4 && bt_conn_enc_key_size(conn) == 16) {
            security_established = true;
            LOG_DBG("Starting Discovery...");
            memcpy(&uuid_128, UUID_AUTH_SERVICE, sizeof(uuid_128));
            discover_params.uuid = &uuid_128.uuid;
            discover_params.func = discover_func;
            discover_params.type = BT_GATT_DISCOVER_PRIMARY;
            discover_params.start_handle = 0x0001;
            discover_params.end_handle = 0xffff;

            int discover_err = bt_gatt_discover(conn, &discover_params);
            if (discover_err) {
                LOG_ERR("Discover failed(err %d)", discover_err);
                return;
            }
        }
    }
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

        if (!bt_uuid_cmp(params->uuid, UUID_AUTH_SERVICE)) {
            LOG_DBG("found auth service handle %u", attr->handle);
            //next up: search challenge chr
            memcpy(&uuid_128, UUID_AUTH_CHALLENGE, sizeof(uuid_128));
            discover_params.uuid = &uuid_128.uuid;
            discover_params.start_handle = attr->handle + 1;
            discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("challenge chr discovery failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else if (!bt_uuid_cmp(params->uuid, UUID_AUTH_CHALLENGE)) {
            LOG_DBG("found auth challenge chr handle %u", attr->handle);
            auth_challenge_chr_value_handle = bt_gatt_attr_value_handle(attr);
            //next up: search response chr
            memcpy(&uuid_128, UUID_AUTH_RESPONSE, sizeof(uuid_128));
            discover_params.uuid = &uuid_128.uuid;
            discover_params.start_handle = attr->handle + 2;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("response chr discovery failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else if (!bt_uuid_cmp(params->uuid, UUID_AUTH_RESPONSE)) {
            LOG_DBG("found auth response chr handle %u", attr->handle);
            auth_response_chr_value_handle = bt_gatt_attr_value_handle(attr);
            subscribe_params.value_handle = auth_response_chr_value_handle;

            //next up: search response chr cccd
            memcpy(&uuid_16, BT_UUID_GATT_CCC, sizeof(uuid_16));
            discover_params.uuid = &uuid_16.uuid;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
            discover_params.start_handle = attr->handle + 2;

            err = bt_gatt_discover(default_conn, &discover_params);
            if (err) {
                LOG_ERR("response chr cccd discovery failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else if (!bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC)) {
            LOG_DBG("found auth response chr cccd handle %u", attr->handle);
            subscribe_params.ccc_handle = attr->handle;
            subscribe_params.value = BT_GATT_CCC_INDICATE;
            subscribe_params.notify = notify_func;

            err = bt_gatt_subscribe(default_conn, &subscribe_params);
            if (err && err != -EALREADY) {
                LOG_ERR("Subscribe failed (err %d)", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                return BT_GATT_ITER_STOP;
            } else {
                LOG_DBG("[SUBSCRIBED]");
            }

            LOG_INF("Discover complete");
            (void) memset(params, 0, sizeof(*params));

            LOG_DBG("Generating new challenge");
            if (bt_rand(challenge, 64) != 0) {
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                return BT_GATT_ITER_STOP;
            }
            LOG_DBG("Writing challenge");
            write_params.func = write_completed_func;
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
static void write_completed_func(struct bt_conn *conn, u8_t err,
                                 struct bt_gatt_write_params *params) {
    ARG_UNUSED(conn);
    ARG_UNUSED(params);
    LOG_DBG("Write complete: err %u", err);
    (void) memset(&write_params, 0, sizeof(write_params));
}

/**
 * check if current response is valid
 * critical section!
 * cleans up and disconnects afterwards.
 * @param conn current connection
 */
static void check_response(struct bt_conn *conn) {
    if (spaceauth_validate(bt_conn_get_dst(conn), challenge, response) == 0) {
        LOG_INF("KEY AUTHENTICATED. OPEN DOOR PLEASE.");
        led0_set(1);
        led1_set(1, 1, 1);
    }
    (void) memset(challenge, 0, sizeof(challenge));
    (void) memset(response, 0, sizeof(response));
    bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
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
    ARG_UNUSED(conn);
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
            read_params.func = read_completed_func;
            read_params.handle_count = 1;
            read_params.single.offset = length;
            read_params.single.handle = auth_response_chr_value_handle;
            int err = bt_gatt_read(default_conn, &read_params);
            if (err) {
                LOG_ERR("Could not read response: %i", err);
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        } else {
            check_response(conn);
            (void) memset(&params, 0, sizeof(params));
            return BT_GATT_ITER_STOP;
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
static u8_t read_completed_func(struct bt_conn *conn, u8_t err,
                                struct bt_gatt_read_params *params,
                                const void *data, u16_t length) {
    if (data) {
        LOG_DBG("Read complete: err %u length %u offset %u", err, length, params->single.offset);
        LOG_HEXDUMP_DBG(data, length, "Received data");
        if (params->single.handle == auth_response_chr_value_handle) {
            if (params->single.offset + length <= sizeof(response)) {
                memcpy(response + params->single.offset, data, length);
                if (params->single.offset + length == sizeof(response)) {
                    check_response(conn);
                    (void) memset(params, 0, sizeof(*params));
                    return BT_GATT_ITER_STOP;
                }
            } else {
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                (void) memset(params, 0, sizeof(*params));
                return BT_GATT_ITER_STOP;
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
    led0_set(0);
    led1_set(0, 0, 0);

    i_want_to_die = true;

    const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    int err;
    if (conn != default_conn) {
        LOG_ERR("Disconnected from unknown connection");
        return;
    }
    LOG_INF("Disconnected: [%02X:%02X:%02X:%02X:%02X:%02X] (reason %u)",
            addr->a.val[5], addr->a.val[4], addr->a.val[3],
            addr->a.val[2], addr->a.val[1], addr->a.val[0],
            reason);

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
    leds_init();
    k_delayed_work_init(&timeout_timer, timeout);

    // install watchdog
    wdt = device_get_binding(DT_WDT_0_NAME);
    if (!wdt) {
        LOG_ERR("Cannot get WDT device");
    }
    wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
    if (wdt_channel_id < 0) {
        LOG_ERR("Watchdog install error");
    } else {
        LOG_INF("Watchdog installed");
    }
    if (wdt_setup(wdt, 0) < 0) {
        LOG_ERR("Watchdog setup error");
    } else {
        LOG_INF("Watchdog setup successful");
    }
    wdt_feed(wdt, wdt_channel_id);

    k_timer_init(&watchdog_timer, watchdog_timer_expiry_function, NULL);
    k_timer_start(&watchdog_timer, K_MSEC(2500), K_MSEC(2500));
}
