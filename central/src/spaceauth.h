#ifdef __cplusplus
extern "C" {
#endif

#include "bluetooth/hci.h"
#include "bluetooth/gatt.h"

#include "guestlist.h"

/**
 * Tells host and controller about the new key
 * @param keys BLE keys
 * @param key space key
 * @return true if successful
 */
bool space_add_id(struct bt_keys *keys, uint8_t *key);

/**
 * Check if remote signature is correct
 * @param addr peripheral address
 * @param challenge
 * @param remote_sign
 * @return true for correct signature
 */
bool check_sign(const bt_addr_le_t * addr, const uint8_t * challenge,const uint8_t * remote_sign);

/**
 * Find corresponding space-key and serialize into file given with arg
 * @param keys
 * @param arg FILE* to write to
 */
void space_save_id(struct bt_keys *keys, void *arg);


#ifdef __cplusplus
}
#endif
