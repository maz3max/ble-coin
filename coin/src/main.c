#include <power.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/crypto.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(app);

//our own stuff
#include "bas.h"
#include "spaceauth.h"
#include "io.h"

/**
 * gets called when system enters a new power state
 * is used for turning the LED off before sleep
 * @param state
 */
void sys_pm_notify_power_state_entry(enum power_states state) {
    if (state == SYS_POWER_STATE_DEEP_SLEEP_1) {
        LOG_INF("entering DEEP SLEEP");
        set_blink_intensity(BI_OFF);
    } else {
        LOG_INF("entering state %i", state);
    }
}

struct bt_conn *default_conn = NULL;

static uint8_t batt_adv_bytes[] = {0x0f, 0x18, /* batt level UUID */
                                   0x00}; /* actual batt level */
// advertising data
static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x00, 0x18, 0x01, 0x18, 0x0f, 0x18),
        BT_DATA(BT_DATA_SVC_DATA16, batt_adv_bytes, sizeof(batt_adv_bytes))
};

/**
 * gets called when the BLE stack is initialized
 * @param err error while initializing
 */
static void bt_ready(int err) {
    if (err) {
        LOG_ERR("bluetooth init failed (err %d)", err);
        return;
    }

    settings_load();

    bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    // bt_foreach_bond(BT_ID_DEFAULT,connect_bonded, NULL);
}

/**
 * gets called when connected to the central
 * @param conn connection
 * @param err error connecting
 */
static void connected(struct bt_conn *conn, u8_t err) {
    if (err) {
        LOG_ERR("connection failed (err %u)", err);
    } else {
        if (default_conn) {
            bt_conn_unref(default_conn);
        }
        default_conn = bt_conn_ref(conn);
        LOG_INF("connected");

        if (bt_conn_security(conn, BT_SECURITY_FIPS)) {
            LOG_INF("Kill connection: insufficient security");
            bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_SECURITY);
        }

        set_blink_intensity(BI_AGGRESSIVE);
    }
}

/**
 * gets called when disconnected from central
 * does cleanup and shutdown
 * @note cleanup is probably not needed
 * @param conn
 * @param reason
 */
static void disconnected(struct bt_conn *conn, u8_t reason) {
    LOG_INF("disconnected (reason %u)", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    LOG_INF("going to sleep");
    sys_pm_force_power_state(SYS_POWER_STATE_DEEP_SLEEP_1);
}

// collection of connection callbacks
static struct bt_conn_cb conn_callbacks = {
        .connected = connected,
        .disconnected = disconnected,
};

static struct k_delayed_work shutdown_timer;

/**
 * shutdown timer callback function
 * @param work
 */
static void shutdown(struct k_work *work) {
    if (default_conn) {
        bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    } else {
        disconnected(NULL, 0);
    }
}

void main(void) {

    // set shutdown timer
    k_delayed_work_init(&shutdown_timer, shutdown);
    k_delayed_work_submit(&shutdown_timer, K_SECONDS(10));
    // initialize own parts
    io_init();
    batt_adv_bytes[2] = bas_init();
    space_auth_init();

    LOG_INF("turning BLE on");
    // enable the bluetooth stack
    int err;
    err = bt_enable(bt_ready);
    if (err) {
        return;
    }
    // set connection and authentication callbacks
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(NULL);
}
