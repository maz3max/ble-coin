#include "helper.h"
#include <logging/log.h>

#include <stdlib.h>
#include <ctype.h>

LOG_MODULE_REGISTER(helper);


int parse_addr(const char *addr, bt_addr_le_t *result) {
    if (strlen(addr) != 17) {
        LOG_ERR("wrong address length");
        return -EINVAL;
    } else {
        for (size_t i = 2; i < 15; i += 3) {
            if (addr[i] != ':') {
                LOG_ERR("invalid address");
                return -EINVAL;
            }
        }
        for (size_t i = 0; i < 6; ++i) {
            size_t off = 3 * i;
            char buf[3] = {addr[off], addr[off + 1], 0};
            if (isxdigit(buf[0]) && isxdigit(buf[1])) {
                result->a.val[5 - i] = strtol(buf, NULL, 16);
            } else {
                LOG_ERR("invalid address");
                return -EINVAL;
            }
        }
    }
    result->type = BT_ADDR_LE_RANDOM;

    return 0;
}

int parse_hex(const char *str, size_t n, uint8_t *out) {
    if (strlen(str) != 2 * n) {
        LOG_ERR("wrong hex string length");
        return -EINVAL;
    }
    for (size_t i = 0; i < n; ++i) {
        char buf[] = {str[2 * i], str[2 * i + 1], 0};
        if (isxdigit(buf[0]) && isxdigit(buf[1])) {
            out[i] = strtol(buf, NULL, 16);
        } else {
            LOG_ERR("invalid hex string");
            return -EINVAL;
        }
    }
    LOG_HEXDUMP_INF(out, n, "parsed data");
    return 0;
}
