#ifndef SPACEAUTH_H
#define SPACEAUTH_H

#include <bluetooth/bluetooth.h>
#include <errno.h>

typedef struct spacekey_t {
    bt_addr_le_t addr;
    uint8_t key[32];
} spacekey_t;

/**
 * For a given address, look up the spacekey struct.
 * @param addr given address
 * @return pointer to spacekey struct if found, NULL otherwise.
 */
spacekey_t *spacekey_lookup(const bt_addr_le_t *addr);

/**
 * Adds a spacekey for a given address.
 * @param addr given address
 * @param key spacekey array
 * @return 0 on success, -ENOSPC if buffer is full, -EINVAL if the address is all-zeroes.
 */
int spacekey_add(const bt_addr_le_t *addr, const uint8_t *key);

/**
 * Deletes a spacekey of a given address.
 * @param addr given address
 * @return 0 on success, -ENOENT if there is no spacekey for this address, -EINVAL if the address is all-zeroes.
 */
int spacekey_del(const bt_addr_le_t *addr);

/**
 * Validates a response to a challenge with the spacekey of the given address.
 * @param addr given address
 * @param challenge challenge sent
 * @param response response received
 * @return 0 on success, -ENOENT if there is no spacekey for this address, -EINVAL if response does not match.
 */
int spaceauth_validate(const bt_addr_le_t *addr, const uint8_t *challenge, const uint8_t *response);

/**
 * Initialize spaceauth settings handler.
 */
void spaceauth_init();

#endif //SPACEAUTH_H
