#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/i2c.h>
#include <gpio.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(app);

#include "kx022.h"

#define I2C_DEV "I2C_0"
#define LED_PORT DT_ALIAS_LED0_GPIOS_CONTROLLER
#define LED DT_ALIAS_LED0_GPIOS_PIN

static uint8_t batt_adv_bytes[] = {0x0f, 0x18, /* batt level UUID */
                                   0x00}; /* actual batt level */
static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA(BT_DATA_SVC_DATA16, batt_adv_bytes, sizeof(batt_adv_bytes))
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

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_INF("Advertising failed to start (err %d)", err);
        return;
    }

    LOG_INF("Advertising successfully started");
}

static u8_t led_state = 0;

static void kx022_int1(struct device *dev, struct gpio_callback *cb, u32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    led_state = 1 - led_state;
    gpio_pin_write(dev, LED, led_state);
    LOG_INF("YEAH, the INT1 interrupt occurred.");
}

static struct gpio_callback kx022_int1_cb = {{0}};

void main(void) {
    struct device *i2c_dev = device_get_binding(I2C_DEV);
    if (!i2c_dev) {
        LOG_INF("I2C: Device driver not found.");
        return;
    }
    batt_adv_bytes[2] = kx022_verify_id(i2c_dev, KX022_ADDR_GND);
    LOG_INF("ID Verified? %d ", batt_adv_bytes[2]);
    kx022_init(i2c_dev, KX022_ADDR_GND);
    u8_t readout[6] = {0};
    while (1) {
        k_sleep(1000);
        kx022_read_acc(i2c_dev, KX022_ADDR_GND, readout);
        int16_t x = (readout[0] | (readout[1] << 8));
        int16_t y = (readout[2] | (readout[3] << 8));
        int16_t z = (readout[4] | (readout[5] << 8));
        x /= 409.6;
        y /= 409.6;
        z /= 409.6;
        LOG_INF("X: %d Y %d Z: %d ", x, y, z);
        //LOG_HEXDUMP_INF(readout, 6, "READOUT");
    }
}
