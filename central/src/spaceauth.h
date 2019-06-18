#ifdef __cplusplus
extern "C" {
#endif

#include "bluetooth/hci.h"
#include "bluetooth/gatt.h"

#include "keys.h"

static struct bt_uuid_128 auth_service_uuid = BT_UUID_INIT_128(
        0xee, 0x8a, 0xcb, 0x07, 0x8d, 0xe1, 0xfc, 0x3b,
        0xfe, 0x8e, 0x69, 0x22, 0x41, 0xbe, 0x87, 0x66);

static struct bt_uuid_128 auth_challenge_uuid = BT_UUID_INIT_128(
        0xd5, 0x12, 0x7b, 0x77, 0xce, 0xba, 0xa7, 0xb1,
        0x86, 0x9a, 0x90, 0x47, 0x02, 0xc9, 0x3d, 0x95);

static struct bt_uuid_128 auth_response_uuid = BT_UUID_INIT_128(
        0x06, 0x3f, 0x0b, 0x51, 0xbf, 0x48, 0x4f, 0x95,
        0x92, 0xd7, 0x28, 0x5c, 0xd6, 0xfd, 0xd2, 0x2f);

/**
 * Tells host and controller about the new key
 * @param keys BLE keys
 * @param key space key
 * @return true if successfull
 */
bool space_add_id(struct bt_keys *keys, uint8_t *key);
bool check_sign(const bt_addr_le_t * addr, const uint8_t * challenge,const uint8_t * remote_sign);


#ifdef __cplusplus
}
#endif
