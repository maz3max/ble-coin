#include <zephyr.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <logging/log.h>
#include <stdlib.h>
#include <ctype.h>

LOG_MODULE_REGISTER(app);

#include <bluetooth/bluetooth.h>
#include <settings/settings.h>

#include <keys.h> //use of internal keys API
#include <settings.h> //use of internal settings API

#include "spaceauth.h"

static int parse_addr(const char *addr, bt_addr_le_t *result) {
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

static int parse_hex(const char *str, const size_t n, uint8_t *out) {
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

static int cmd_coin_add(const struct shell *shell, size_t argc, char **argv) {
    shell_print(shell, "argc = %d", argc);
    for (size_t cnt = 0; cnt < argc; cnt++) {
        shell_print(shell, "  argv[%d] = %s", cnt, argv[cnt]);
    }
    struct bt_keys keys = {
            .keys = (BT_KEYS_IRK | BT_KEYS_LTK_P256),
            .flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC),
            .id = BT_ID_DEFAULT,
            .enc_size = BT_ENC_KEY_SIZE_MAX,
    };
    uint8_t spacekey[32] = {0};

    if (argc != 5) {
        LOG_ERR("incorrect number of arguments");
        return EINVAL;
    }

    // address
    int ret = parse_addr(argv[1], &keys.addr);
    if (ret) {
        return ret;
    }
    LOG_INF("valid address");
    // identity resolving key
    ret = parse_hex(argv[2], 16, keys.irk.val);
    if (ret) {
        return ret;
    }
    LOG_INF("valid IRK");
    // long-term key
    ret = parse_hex(argv[3], 16, keys.ltk.val);
    if (ret) {
        return ret;
    }
    LOG_INF("valid LTK");
    // space key
    ret = parse_hex(argv[4], 32, spacekey);
    if (ret) {
        return ret;
    }
    LOG_INF("valid space key");

    //TODO register bt_keys and spacekey
    char key[BT_SETTINGS_KEY_MAX];
    bt_settings_encode_key(key, sizeof(key), "keys", &keys.addr, NULL);
    bt_keys_store(&keys);
    settings_load_subtree(key);
    spacekey_add(&keys.addr, spacekey);
    //TODO restart discovery
    return 0;
}

static int cmd_coin_del(const struct shell *shell, size_t argc, char **argv) {
    shell_print(shell, "argc = %d", argc);
    for (size_t cnt = 0; cnt < argc; cnt++) {
        shell_print(shell, "  argv[%d] = %s", cnt, argv[cnt]);
    }
    bt_addr_le_t addr;
    if (argc != 2) {
        LOG_ERR("incorrect number of arguments");
        return EINVAL;
    }
    int ret = parse_addr(argv[1], &addr);
    if (ret) {
        return ret;
    }
    LOG_INF("valid address");
    char addr_str[BT_ADDR_LE_STR_LEN] = "foo";
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    shell_print(shell, "parsed BLE addr [%s]", addr_str);

    //TODO unpair bt_keys and spacekey
    ret = bt_unpair(BT_ID_DEFAULT, &addr);
    if (ret) {
        LOG_ERR("could not unpair this address (err %d)", ret);
    }
    spacekey_del(&addr);

    return 0;
}

/* Creating subcommands (level 1 command) array for command "coin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_coin,
                               SHELL_CMD(add, NULL, "usage: coin add <addr> <irk> <ltk> <spacekey>",
                                         cmd_coin_add),
                               SHELL_CMD(del, NULL, "usage: coin del <addr> ", cmd_coin_del),
                               SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "coin" */
SHELL_CMD_REGISTER(coin, &sub_coin, "commands to manage coins", NULL);

static int cmd_print_spacekeys(const struct shell *shell, size_t argc, char **argv) {
    spacekeys_print(shell);
}

static void print_key(struct bt_keys *keys, void *data) {
    const struct shell *shell = data;
    shell_print(shell, "[%s] keys: %u, flags: %u", bt_addr_le_str(&keys->addr), keys->keys, keys->flags);
}

static int cmd_print_bonds(const struct shell *shell, size_t argc, char **argv) {
    shell_print(shell, "printing all spacekeys...");
    bt_keys_foreach(BT_KEYS_ALL, print_key, shell);
}


/* Creating subcommands (level 1 command) array for command "coin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_stats,
                               SHELL_CMD(spacekey, NULL, "prints space keys", cmd_print_spacekeys),
                               SHELL_CMD(bonds, NULL, "prints bonds", cmd_print_bonds),
                               SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "coin" */
SHELL_CMD_REGISTER(stats, &sub_stats, "commands to print internal state", NULL);

void main(void) {
    spaceauth_init();
}
