#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/i2c.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#define I2C_DEV "I2C_0"

#define KX022_ADDR 0b0011110

static uint8_t batt_adv_bytes[] = {0x0f, 0x18, /* batt level UUID */
                                   0x00}; /* actual batt level */
static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA(BT_DATA_SVC_DATA16, batt_adv_bytes, sizeof(batt_adv_bytes))
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

void main(void)
{

	printk("Starting i2c scanner...\n");

	
	struct device *i2c_dev = device_get_binding(I2C_DEV);
	if (!i2c_dev) {
		printk("I2C: Device driver not found.\n");
		return;
	}
	for (u8_t i = 4; i <= 0x77; i++) {
		struct i2c_msg msgs[1];
		u8_t dst;

		msgs[0].buf = &dst;
		msgs[0].len = 0U;
		msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

		if (i2c_transfer(i2c_dev, &msgs[0], 1, i) == 0) {
			printk("0x%2x FOUND\n", i);
                        batt_adv_bytes[2] = i;
		}
	} 
	int err = bt_enable(bt_ready);
	if (err) {
		 printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
}
