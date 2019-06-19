/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <gpio.h>
#include <power.h>
#include <soc.h>
#include <adc.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/crypto.h>
#include <keys.h>


#include "bas.h"
#include "spaceauth.h"

#define LED_PORT LED0_GPIO_CONTROLLER
#define LED    LED0_GPIO_PIN
#define BTN    SW0_GPIO_PIN
#define BTN_PORT SW0_GPIO_CONTROLLER

#include <hal/nrf_saadc.h>

#define ADC_DEVICE_NAME        DT_ADC_0_NAME
#define ADC_RESOLUTION        10
#define ADC_GAIN        ADC_GAIN_1_3
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME    ADC_ACQ_TIME_DEFAULT
#define ADC_1ST_CHANNEL_ID    0
#define ADC_1ST_CHANNEL_INPUT    NRF_SAADC_INPUT_AIN0
#define BAT_LOW            3

static const struct adc_channel_cfg m_1st_channel_cfg = {
        .gain             = ADC_GAIN,
        .reference        = ADC_REFERENCE,
        .acquisition_time = ADC_ACQUISITION_TIME,
        .channel_id       = ADC_1ST_CHANNEL_ID,
        .input_positive   = ADC_1ST_CHANNEL_INPUT,
};

/** Button and Deep Sleep stuff
 *
 */

static struct gpio_callback btn_cb;

void sys_pm_notify_power_state_entry(enum power_states state) {
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

void button_pressed(struct device *dev, struct gpio_callback *cb,
                    u32_t pins) {
    //do nothing (yet)
}

/** ADC stuff
 *
 */

static struct device *init_adc(void) {
    int ret;
    struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);

    if (!adc_dev) {
        printk("couldn't get ADC dev binding\n");
    }

    ret = adc_channel_setup(adc_dev, &m_1st_channel_cfg);
    if (ret != 0) {
        printk("adc channel config failed with code %i", ret);
    }

    return adc_dev;
}

nrf_saadc_value_t my_read_adc(struct device *dev, struct device *adc_dev) {
    gpio_pin_configure(dev, BAT_LOW, GPIO_DIR_OUT);
    gpio_pin_write(dev, BAT_LOW, 0);
    if (!adc_dev) {
        printk("OH NOES! NO ADC AVAILABLE!\n");
    } else {
        int ret;
        nrf_saadc_value_t val;

        const struct adc_sequence sequence = {
                .channels    = BIT(ADC_1ST_CHANNEL_ID),
                .buffer      = &val,
                .buffer_size = sizeof(nrf_saadc_value_t),
                .resolution  = ADC_RESOLUTION,
                .oversampling = true,
        };
        ret = adc_read(adc_dev, &sequence);
        if (ret == 0) {
            return val;
        } else {
            printk("ADC read failed with code %i\n", ret);
            return -1;
        }
    }
    gpio_pin_configure(dev, BAT_LOW, GPIO_DIR_IN);
}

void intialise_battery() {
    struct device *dev = device_get_binding(LED_PORT);
    struct device *adc_dev = init_adc();
    nrf_saadc_value_t val = my_read_adc(dev, adc_dev);
    //int voltage = (int) (3.5156249999999997 * (val));
    //printk("COUNTS: %i, mVolts: %i\n", val, voltage);
    float batt_percentage_f = 0.35156249999999997 * val - 200;
    u8_t batt_percentage = batt_percentage_f < 0 ? 0 : batt_percentage_f > 100 ? 100 : batt_percentage_f;
    bas_init(batt_percentage);
}

//BLE stuff
static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static void bt_ready(int err) {
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    settings_load();

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising successfully started\n");
}

struct bt_conn *default_conn;

static void connected(struct bt_conn *conn, u8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        default_conn = bt_conn_ref(conn);
        printk("Connected\n");
        if (bt_conn_security(conn, BT_SECURITY_FIPS)) {
            printk("Kill connection: insufficient security\n");
            bt_conn_disconnect(conn, 0x0E);
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

static void shutdown(struct k_work *work) {
    disconnected(NULL, 0);
}

void main(void) {
    struct device *dev = device_get_binding(LED_PORT);

    printk("Hello World!\n");

    //set shutdown timer
    k_delayed_work_init(&shutdown_timer, shutdown);
    k_delayed_work_submit(&shutdown_timer, K_SECONDS(30));

    /* Set LED pin as output */
    gpio_pin_configure(dev, LED, GPIO_DIR_OUT);
    gpio_pin_configure(dev, BTN, GPIO_DIR_IN | GPIO_PUD_PULL_UP | GPIO_INT_LEVEL | GPIO_INT | GPIO_INT_ACTIVE_LOW);
    gpio_init_callback(&btn_cb, button_pressed, BIT(BTN));
    gpio_add_callback(dev, &btn_cb);
    gpio_pin_enable_callback(dev, BTN);

    gpio_pin_write(dev, LED, 1);

    sys_pm_force_power_state(SYS_POWER_STATE_ACTIVE);

    intialise_battery();
    space_auth_init();

    //enable the bluetooth stack
    int err;
    err = bt_enable(bt_ready);
    if (err) {
        return;
    }
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(NULL);

    settings_commit();

    bt_addr_le_t id;
    bt_addr_le_create_static(&id);
    //k_sleep(10000);
    //sys_pm_force_power_state(SYS_POWER_STATE_DEEP_SLEEP_1);
}
