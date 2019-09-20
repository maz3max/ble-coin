#include <zephyr.h>

#include <bluetooth/gatt.h>

#include <device.h>
#include <gpio.h>
#include <adc.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(bas);

static u8_t battery = 0U;

static ssize_t read_blvl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, u16_t len, u16_t offset) {
    battery = 0;

    const char *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                             sizeof(*value));
}

/* Battery Service Declaration */
BT_GATT_SERVICE_DEFINE(bas_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL,
                                              BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ, read_blvl, NULL, &battery),
);

uint8_t bas_init(){
    LOG_INF("initialize battery service");
    battery = 0;
    return battery;
}
