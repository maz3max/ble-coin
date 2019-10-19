#pragma once

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

/**
 * Notify helper functions that BLE stack is running.
 * (disables write-commands to settings)
 */
void helper_ble_running();