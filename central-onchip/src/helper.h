#ifndef HELPER_H
#define HELPER_H

#include <bluetooth/bluetooth.h>

/**
 * Parses BLE random address from string
 * @param addr address string
 * @param result output address
 * @return 0 on success, -EINVAL on invalid input
 */
int parse_addr(const char *addr, bt_addr_le_t *result);

/**
 * Parses n bytes of hex string
 * @param str hex string
 * @param n number of bytes to read
 * @param out output buffer
 * @return 0 on success, -EINVAL on invalid input
 */
int parse_hex(const char *str, size_t n, uint8_t *out);

#endif //HELPER_H
