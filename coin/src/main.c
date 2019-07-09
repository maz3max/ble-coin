#include <gpio.h>
#include <power.h>
//#include <soc.h>
//#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/crypto.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>

//our own stuff
#include "bas.h"
#include "spaceauth.h"

#define LED_PORT LED0_GPIO_CONTROLLER
#define LED LED0_GPIO_PIN
#define BTN SW0_GPIO_PIN
#define BTN_PORT SW0_GPIO_CONTROLLER


/** Button and Deep Sleep stuff
 *
 */

static struct gpio_callback btn_cb = {0};

void sys_pm_notify_power_state_entry(enum power_states state) {
    settings_save();
    if (state == SYS_POWER_STATE_DEEP_SLEEP_1) {
        printk("Entering DEEP SLEEP\n");
        struct device *dev = device_get_binding(LED_PORT);
        gpio_pin_write(dev, LED, 0);
    } else {
        printk("Entering state %i", state);
    }
}

void sys_pm_notify_power_state_exit(enum power_states state) {
    printk("Exiting power state: %i\n", (int) state);
}

void button_pressed(struct device *dev, struct gpio_callback *cb, u32_t pins) {
    // do nothing (yet)
}

// BLE stuff

struct bt_conn *default_conn = NULL;

static void connect_bonded(const struct bt_bond_info *info, void *user_data) {
    if (default_conn) {
        bt_conn_unref(default_conn);
    }
    default_conn = bt_conn_create_slave_le(&info->addr, BT_LE_ADV_CONN);
    if (!default_conn) {
        printk("Error advertising\n");
    }
}

static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static void bt_ready(int err) {
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    settings_load();

    bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
}

static void connected(struct bt_conn *conn, u8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        if (default_conn) {
            bt_conn_unref(default_conn);
        }
        default_conn = bt_conn_ref(conn);
        printk("Connected\n");
        if (bt_conn_security(conn, BT_SECURITY_FIPS)) {
            printk("Kill connection: insufficient security\n");
            bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_SECURITY);
        }
    }
}

static void disconnected(struct bt_conn *conn, u8_t reason) {
    printk("Disconnected (reason %u)\n", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    printk("Going to sleep\n");
    sys_pm_force_power_state(SYS_POWER_STATE_DEEP_SLEEP_1);
}

static struct bt_conn_cb conn_callbacks = {
        .connected = connected,
        .disconnected = disconnected,
};

static struct k_delayed_work shutdown_timer;

static void shutdown(struct k_work *work) { disconnected(NULL, 0); }

void main(void) {
    struct device *dev = device_get_binding(LED_PORT);

    printk("Hello World!\n");

    // set shutdown timer
    k_delayed_work_init(&shutdown_timer, shutdown);
    k_delayed_work_submit(&shutdown_timer, K_SECONDS(30));

    /* Set LED pin as output */
    gpio_pin_configure(dev, LED, GPIO_DIR_OUT);
    gpio_pin_configure(dev, BTN,
                       GPIO_DIR_IN | GPIO_PUD_PULL_UP | GPIO_INT_LEVEL |
                       GPIO_INT | GPIO_INT_ACTIVE_LOW);
    gpio_init_callback(&btn_cb, button_pressed, BIT(BTN));
    gpio_add_callback(dev, &btn_cb);
    gpio_pin_enable_callback(dev, BTN);

    gpio_pin_write(dev, LED, 1);

    sys_pm_force_power_state(SYS_POWER_STATE_ACTIVE);

    bas_init();
    space_auth_init();

  // enable the bluetooth stack
  int err;
  err = bt_enable(bt_ready);
  if (err) {
    return;
  }
  bt_conn_cb_register(&conn_callbacks);
  bt_conn_auth_cb_register(NULL);
}
