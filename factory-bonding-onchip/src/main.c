#include <adc.h>
#include <device.h>
#include <gpio.h>
#include <power.h>
#include <soc.h>
#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/crypto.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <keys.h>

#include "main.h"

void main(void) {

  bt_addr_le_t central_addr = INSERT_CENTRAL_ADDR_HERE;
  struct bt_irk central_irk = {
      .val = INSERT_CENTRAL_IRK_HERE,
      .rpa = {0}
  };
  bt_addr_le_t periph_addr = INSERT_PERIPH_ADDR_HERE;
  struct bt_irk periph_irk = {
      .val = INSERT_PERIPH_IRK_HERE,
      .rpa = {0}
  };
  struct bt_keys periph_keys = {
      .id = BT_ID_DEFAULT,
      .addr = central_addr,
      .irk = central_irk,
      .enc_size = BT_ENC_KEY_SIZE_MAX,
      .flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC),
      .keys = (BT_KEYS_IRK | BT_KEYS_LTK_P256),
      .ltk = {.rand = {0},
              .ediv = {0},
              .val = INSERT_LTK_HERE},
  };
  uint8_t spacekey[] = INSERT_SPACEKEY_HERE;

  settings_subsys_init();
  printk("Saving ID\n");
  settings_save_one("bt/id", &periph_addr, sizeof(periph_addr));
  printk("Saving IRK\n");
  settings_save_one("bt/irk", &periph_irk, sizeof(periph_irk));
  printk("Saving KEYS\n");
  bt_keys_store(&periph_keys);
  printk("Saving SpaceKey\n");
  settings_save_one("space/key", &spacekey, sizeof(spacekey));
  printk("Saving Complete!\n");
}
